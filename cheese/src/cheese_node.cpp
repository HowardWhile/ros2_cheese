#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#if __has_include("cv_bridge/cv_bridge.hpp")
#include "cv_bridge/cv_bridge.hpp"
#elif __has_include("cv_bridge/cv_bridge.h")
#include "cv_bridge/cv_bridge.h"
#else
#error "cv_bridge header not found"
#endif
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/image_encodings.hpp"
#include "sensor_msgs/msg/compressed_image.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_srvs/srv/trigger.hpp"

#include "cheese/json.hpp"

namespace
{
constexpr auto kTopicProbePeriod = std::chrono::milliseconds(1000);
constexpr auto kStatusPublishPeriod = std::chrono::milliseconds(1000);
constexpr auto kStatusLogPeriod = std::chrono::seconds(10);
constexpr auto kStreamTimeout = std::chrono::milliseconds(2000);
constexpr int64_t kBytesPerMegabyte = 1024LL * 1024LL;
constexpr char kRawImageType[] = "sensor_msgs/msg/Image";
constexpr char kCompressedImageType[] = "sensor_msgs/msg/CompressedImage";
const std::filesystem::path kDefaultCaptureDir{"/tmp/ros2_cheese"};

using Json = nlohmann::json;

std::string sanitize_topic_for_log(const std::string &topic)
{
    return topic.empty() ? std::string("<empty>") : topic;
}
}  // namespace

class CheeseNode : public rclcpp::Node
{
public:
    CheeseNode() : Node("cheese")
    {
        image_topic_ = declare_parameter<std::string>("image_topic", "/camera/color/image_raw");
        capture_dir_ = declare_parameter<std::string>("capture_dir", kDefaultCaptureDir.string());
        max_files_ = declare_parameter<int>("max_files", 1000);
        max_mb_ = declare_parameter<int64_t>("max_mb", 1024);

        if (max_files_ < 0)
        {
            RCLCPP_WARN(get_logger(), "max_files must be >= 0; using unlimited file count");
            max_files_ = 0;
        }
        if (max_mb_ < 0)
        {
            RCLCPP_WARN(get_logger(), "max_mb must be >= 0; using unlimited storage size");
            max_mb_ = 0;
        }
        max_bytes_ = max_mb_ * kBytesPerMegabyte;

        std::filesystem::create_directories(capture_dir_);

        trigger_srv_ = create_service<std_srvs::srv::Trigger>(
            "trigger", std::bind(&CheeseNode::handleTrigger, this, std::placeholders::_1, std::placeholders::_2));
        status_pub_ = create_publisher<std_msgs::msg::String>("status", 10);

        topic_probe_timer_ = create_wall_timer(kTopicProbePeriod, std::bind(&CheeseNode::probeImageTopic, this));
        status_timer_ = create_wall_timer(kStatusPublishPeriod, std::bind(&CheeseNode::publishStatus, this));
        last_status_time_ = now();
        last_status_log_time_ = last_status_time_;
        probeImageTopic();
        pruneCaptures();

        RCLCPP_INFO(get_logger(), "Cheese service: ~/trigger");
        RCLCPP_INFO(get_logger(), "Cheese status topic: ~/status");
        RCLCPP_INFO(get_logger(), "Cheese image topic: %s", sanitize_topic_for_log(image_topic_).c_str());
        RCLCPP_INFO(get_logger(), "Cheese capture dir: %s", capture_dir_.string().c_str());
        RCLCPP_INFO(get_logger(), "Cheese limits: max_files=%d, max_mb=%ld (%ld bytes)", max_files_,
                    static_cast<long>(max_mb_), static_cast<long>(max_bytes_));
    }

private:
    enum class ImageTopicKind
    {
        kUnknown,
        kRaw,
        kCompressed
    };

    struct CaptureFile
    {
        std::filesystem::path path;
        std::filesystem::file_time_type modified_time;
        uintmax_t size = 0;
    };

    struct StreamStats
    {
        double fps_min = std::numeric_limits<double>::infinity();
        double fps_max = 0.0;
        double fps_sum = 0.0;
        double bandwidth_mbps_min = std::numeric_limits<double>::infinity();
        double bandwidth_mbps_max = 0.0;
        double bandwidth_mbps_sum = 0.0;
        uint64_t sample_count = 0;
    };

    struct CaptureDirStats
    {
        bool exists = false;
        uint64_t file_count = 0;
        uintmax_t total_bytes = 0;
    };

    void probeImageTopic()
    {
        const auto detected_kind = detectImageTopicKind();
        if (!detected_kind.has_value())
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "Waiting for image topic '%s' with type %s or %s",
                                 image_topic_.c_str(), kRawImageType, kCompressedImageType);
            return;
        }

        if (detected_kind.value() == subscribed_kind_)
        {
            return;
        }

        raw_sub_.reset();
        compressed_sub_.reset();
        subscribed_kind_ = detected_kind.value();

        if (subscribed_kind_ == ImageTopicKind::kRaw)
        {
            raw_sub_ = create_subscription<sensor_msgs::msg::Image>(
                image_topic_, rclcpp::SensorDataQoS(),
                std::bind(&CheeseNode::rawImageCallback, this, std::placeholders::_1));
            RCLCPP_INFO(get_logger(), "Subscribed to raw image topic: %s", image_topic_.c_str());
        }
        else if (subscribed_kind_ == ImageTopicKind::kCompressed)
        {
            compressed_sub_ = create_subscription<sensor_msgs::msg::CompressedImage>(
                image_topic_, rclcpp::SensorDataQoS(),
                std::bind(&CheeseNode::compressedImageCallback, this, std::placeholders::_1));
            RCLCPP_INFO(get_logger(), "Subscribed to compressed image topic: %s", image_topic_.c_str());
        }
    }

    std::optional<ImageTopicKind> detectImageTopicKind() const
    {
        const auto topics = get_topic_names_and_types();
        const auto topic_it = topics.find(image_topic_);
        if (topic_it == topics.end())
        {
            return std::nullopt;
        }

        const auto &types = topic_it->second;
        if (std::find(types.begin(), types.end(), kRawImageType) != types.end())
        {
            return ImageTopicKind::kRaw;
        }
        if (std::find(types.begin(), types.end(), kCompressedImageType) != types.end())
        {
            return ImageTopicKind::kCompressed;
        }

        return std::nullopt;
    }

    void rawImageCallback(const sensor_msgs::msg::Image::SharedPtr msg)
    {
        try
        {
            const cv_bridge::CvImageConstPtr cv_ptr = cv_bridge::toCvShare(msg);
            cv::Mat bgr_image;

            if (msg->encoding == sensor_msgs::image_encodings::BGR8)
            {
                bgr_image = cv_ptr->image;
            }
            else if (msg->encoding == sensor_msgs::image_encodings::RGB8)
            {
                cv::cvtColor(cv_ptr->image, bgr_image, cv::COLOR_RGB2BGR);
            }
            else if (msg->encoding == sensor_msgs::image_encodings::MONO8)
            {
                cv::cvtColor(cv_ptr->image, bgr_image, cv::COLOR_GRAY2BGR);
            }
            else
            {
                bgr_image = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8)->image;
            }

            updateLatestImage(bgr_image);
            recordImageSample(rawImageByteSize(*msg));
        }
        catch (const std::exception &exc)
        {
            recordImageFailure();
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "Failed to convert raw image: %s", exc.what());
        }
    }

    void compressedImageCallback(const sensor_msgs::msg::CompressedImage::SharedPtr msg)
    {
        const cv::Mat encoded_image(1, static_cast<int>(msg->data.size()), CV_8UC1,
                                    const_cast<uint8_t *>(msg->data.data()));
        const cv::Mat decoded_image = cv::imdecode(encoded_image, cv::IMREAD_COLOR);
        if (decoded_image.empty())
        {
            recordImageFailure();
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "Failed to decode compressed image");
            return;
        }

        updateLatestImage(decoded_image);
        recordImageSample(msg->data.size());
    }

    void updateLatestImage(const cv::Mat &image)
    {
        std::lock_guard<std::mutex> lock(image_mutex_);
        latest_image_ = image.clone();
    }


    static size_t rawImageByteSize(const sensor_msgs::msg::Image &msg)
    {
        if (!msg.data.empty())
        {
            return msg.data.size();
        }

        return static_cast<size_t>(msg.step) * static_cast<size_t>(msg.height);
    }

    void recordImageSample(const size_t byte_count)
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        ++window_frame_count_;
        window_byte_count_ += byte_count;
        last_image_time_ = now();
        has_received_image_ = true;
    }

    void recordImageFailure()
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        ++window_failure_count_;
        ++total_failure_count_;
    }

    bool isSubscribed() const
    {
        return subscribed_kind_ == ImageTopicKind::kRaw || subscribed_kind_ == ImageTopicKind::kCompressed;
    }

    std::string subscribedKindName() const
    {
        switch (subscribed_kind_)
        {
            case ImageTopicKind::kRaw:
                return "raw";
            case ImageTopicKind::kCompressed:
                return "compressed";
            case ImageTopicKind::kUnknown:
                return "unknown";
        }

        return "unknown";
    }

    void publishStatus()
    {
        const auto current_time = now();
        const auto elapsed = std::max(1e-9, (current_time - last_status_time_).seconds());
        last_status_time_ = current_time;

        uint64_t frame_count = 0;
        uint64_t byte_count = 0;
        uint64_t failure_count = 0;
        uint64_t total_failure_count = 0;
        bool has_received_image = false;
        rclcpp::Time last_image_time;

        {
            std::lock_guard<std::mutex> lock(stream_mutex_);
            frame_count = window_frame_count_;
            byte_count = window_byte_count_;
            failure_count = window_failure_count_;
            total_failure_count = total_failure_count_;
            has_received_image = has_received_image_;
            last_image_time = last_image_time_;
            window_frame_count_ = 0;
            window_byte_count_ = 0;
            window_failure_count_ = 0;
        }

        const double fps = static_cast<double>(frame_count) / elapsed;
        const double bandwidth_mbps = (static_cast<double>(byte_count) * 8.0) / elapsed / 1000000.0;
        updateStreamStats(fps, bandwidth_mbps);

        const bool subscribed = isSubscribed();
        const double seconds_since_last_image = has_received_image ? (current_time - last_image_time).seconds() : -1.0;
        const bool stream_timeout =
            !has_received_image || seconds_since_last_image > std::chrono::duration<double>(kStreamTimeout).count();
        const bool stream_ok = subscribed && !stream_timeout && failure_count == 0;
        const auto capture_dir_stats = collectCaptureDirStats();

        Json status;
        status["image_topic"] = image_topic_;
        status["subscribed"] = subscribed;
        status["subscription_type"] = subscribedKindName();
        status["stream_ok"] = stream_ok;
        status["stream_abnormal"] = !stream_ok;
        status["seconds_since_last_image"] = seconds_since_last_image;
        status["window"]["seconds"] = elapsed;
        status["window"]["frames"] = frame_count;
        status["window"]["bytes"] = byte_count;
        status["window"]["failures"] = failure_count;
        status["total_failures"] = total_failure_count;
        status["capture_dir"]["path"] = capture_dir_.string();
        status["capture_dir"]["exists"] = capture_dir_stats.exists;
        status["capture_dir"]["files"] = capture_dir_stats.file_count;
        status["capture_dir"]["bytes"] = capture_dir_stats.total_bytes;
        status["capture_dir"]["mb"] = static_cast<double>(capture_dir_stats.total_bytes) / kBytesPerMegabyte;
        status["capture_dir"]["max_files"] = max_files_;
        status["capture_dir"]["max_mb"] = max_mb_;
        status["capture_dir"]["max_bytes"] = max_bytes_;
        status["fps"] = buildStatsJson(fps, stream_stats_.fps_min, stream_stats_.fps_sum, stream_stats_.fps_max,
                                        stream_stats_.sample_count);
        status["bandwidth_mbps"] =
            buildStatsJson(bandwidth_mbps, stream_stats_.bandwidth_mbps_min, stream_stats_.bandwidth_mbps_sum,
                           stream_stats_.bandwidth_mbps_max, stream_stats_.sample_count);

        const auto status_text = status.dump();

        std_msgs::msg::String msg;
        msg.data = status_text;
        status_pub_->publish(msg);

        if ((current_time - last_status_log_time_).seconds() >=
            std::chrono::duration<double>(kStatusLogPeriod).count())
        {
            last_status_log_time_ = current_time;
            const auto fps_avg = stream_stats_.sample_count == 0
                                     ? 0.0
                                     : stream_stats_.fps_sum / static_cast<double>(stream_stats_.sample_count);
            const auto bandwidth_avg = stream_stats_.sample_count == 0
                                           ? 0.0
                                           : stream_stats_.bandwidth_mbps_sum /
                                                 static_cast<double>(stream_stats_.sample_count);
            RCLCPP_INFO(get_logger(),
                        "\n"
                        "--- [CHEESE STATUS REPORT] ---\n"
                        "Topic: %s | Subscribed: %s | Type: %s\n"
                        "Stream OK: %s | Abnormal: %s | Last Image: %.3f sec | Failures: %lu\n"
                        "FPS Current/Min/Avg/Max: %.1f / %.1f / %.1f / %.1f\n"
                        "BW Mbps Current/Min/Avg/Max: %.1f / %.1f / %.1f / %.1f\n"
                        "Capture Dir: %s | Exists: %s | Files: %lu / %d | Size: %.2f MB / %ld MB\n"
                        "------------------------------",
                        image_topic_.c_str(), subscribed ? "True" : "False", subscribedKindName().c_str(),
                        stream_ok ? "True" : "False", stream_ok ? "False" : "True", seconds_since_last_image,
                        static_cast<unsigned long>(total_failure_count), fps, stream_stats_.fps_min, fps_avg,
                        stream_stats_.fps_max, bandwidth_mbps, stream_stats_.bandwidth_mbps_min, bandwidth_avg,
                        stream_stats_.bandwidth_mbps_max, capture_dir_.string().c_str(),
                        capture_dir_stats.exists ? "True" : "False",
                        static_cast<unsigned long>(capture_dir_stats.file_count), max_files_,
                        static_cast<double>(capture_dir_stats.total_bytes) / kBytesPerMegabyte,
                        static_cast<long>(max_mb_));
        }
    }

    void updateStreamStats(const double fps, const double bandwidth_mbps)
    {
        ++stream_stats_.sample_count;
        stream_stats_.fps_min = std::min(stream_stats_.fps_min, fps);
        stream_stats_.fps_max = std::max(stream_stats_.fps_max, fps);
        stream_stats_.fps_sum += fps;
        stream_stats_.bandwidth_mbps_min = std::min(stream_stats_.bandwidth_mbps_min, bandwidth_mbps);
        stream_stats_.bandwidth_mbps_max = std::max(stream_stats_.bandwidth_mbps_max, bandwidth_mbps);
        stream_stats_.bandwidth_mbps_sum += bandwidth_mbps;
    }

    Json buildStatsJson(const double current, const double min_value, const double sum, const double max_value,
                        const uint64_t sample_count) const
    {
        Json stats;
        stats["current"] = current;
        stats["min"] = sample_count == 0 ? 0.0 : min_value;
        stats["avg"] = sample_count == 0 ? 0.0 : sum / static_cast<double>(sample_count);
        stats["max"] = max_value;
        return stats;
    }


    CaptureDirStats collectCaptureDirStats() const
    {
        CaptureDirStats stats;
        std::error_code ec;
        stats.exists = std::filesystem::exists(capture_dir_, ec) && std::filesystem::is_directory(capture_dir_, ec);
        if (!stats.exists)
        {
            return stats;
        }

        for (const auto &entry : std::filesystem::directory_iterator(capture_dir_, ec))
        {
            if (ec || !entry.is_regular_file())
            {
                continue;
            }

            const auto size = entry.file_size(ec);
            if (ec)
            {
                ec.clear();
                continue;
            }

            ++stats.file_count;
            stats.total_bytes += size;
        }

        return stats;
    }

    void handleTrigger(const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
                       std::shared_ptr<std_srvs::srv::Trigger::Response> response)
    {
        (void)request;

        cv::Mat image;
        {
            std::lock_guard<std::mutex> lock(image_mutex_);
            if (!latest_image_.empty())
            {
                image = latest_image_.clone();
            }
        }

        if (image.empty())
        {
            response->success = false;
            response->message = "No image has been received yet";
            return;
        }

        try
        {
            std::filesystem::create_directories(capture_dir_);
            const auto path = capture_dir_ / nextImageFilename();
            if (!cv::imwrite(path.string(), image))
            {
                response->success = false;
                response->message = "Failed to write image: " + path.string();
                return;
            }

            pruneCaptures();
            response->success = true;
            response->message = path.string();
            RCLCPP_INFO(get_logger(), "Saved image: %s", path.string().c_str());
        }
        catch (const std::exception &exc)
        {
            response->success = false;
            response->message = std::string("Failed to save image: ") + exc.what();
            RCLCPP_ERROR(get_logger(), "%s", response->message.c_str());
        }
    }

    std::string nextImageFilename() const
    {
        const auto now = std::chrono::system_clock::now();
        const auto now_time_t = std::chrono::system_clock::to_time_t(now);
        const auto subseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

        std::tm tm_snapshot;
        localtime_r(&now_time_t, &tm_snapshot);

        std::ostringstream stream;
        stream << "cheese-" << std::put_time(&tm_snapshot, "%Y%m%d-%H%M%S") << "-" << std::setfill('0') << std::setw(3)
               << subseconds << ".jpg";
        return stream.str();
    }

    void pruneCaptures()
    {
        if (max_files_ == 0 && max_bytes_ == 0)
        {
            return;
        }

        std::vector<CaptureFile> files;
        uintmax_t total_bytes = 0;

        for (const auto &entry : std::filesystem::directory_iterator(capture_dir_))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            std::error_code ec;
            const auto size = entry.file_size(ec);
            if (ec)
            {
                continue;
            }

            files.push_back(CaptureFile{entry.path(), entry.last_write_time(ec), size});
            total_bytes += size;
        }

        std::sort(files.begin(), files.end(),
                  [](const CaptureFile &lhs, const CaptureFile &rhs)
                  {
                      return lhs.modified_time < rhs.modified_time;
                  });

        auto file_count = files.size();
        for (const auto &file : files)
        {
            const bool too_many_files = max_files_ > 0 && file_count > static_cast<size_t>(max_files_);
            const bool too_many_bytes = max_bytes_ > 0 && total_bytes > static_cast<uintmax_t>(max_bytes_);
            if (!too_many_files && !too_many_bytes)
            {
                break;
            }

            std::error_code ec;
            if (std::filesystem::remove(file.path, ec))
            {
                --file_count;
                total_bytes -= std::min(total_bytes, file.size);
                RCLCPP_INFO(get_logger(), "Pruned old capture: %s", file.path.string().c_str());
            }
            else if (ec)
            {
                RCLCPP_WARN(get_logger(), "Failed to prune old capture '%s': %s", file.path.string().c_str(),
                            ec.message().c_str());
            }
        }
    }

    std::string image_topic_;
    std::filesystem::path capture_dir_;
    int max_files_ = 1000;
    int64_t max_mb_ = 1024;
    int64_t max_bytes_ = 1024LL * kBytesPerMegabyte;
    ImageTopicKind subscribed_kind_ = ImageTopicKind::kUnknown;
    cv::Mat latest_image_;
    std::mutex image_mutex_;
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr raw_sub_;
    rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr compressed_sub_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr status_pub_;
    rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr trigger_srv_;
    rclcpp::TimerBase::SharedPtr topic_probe_timer_;
    rclcpp::TimerBase::SharedPtr status_timer_;
    rclcpp::Time last_status_time_;
    rclcpp::Time last_status_log_time_;
    rclcpp::Time last_image_time_;
    std::mutex stream_mutex_;
    uint64_t window_frame_count_ = 0;
    uint64_t window_byte_count_ = 0;
    uint64_t window_failure_count_ = 0;
    uint64_t total_failure_count_ = 0;
    bool has_received_image_ = false;
    StreamStats stream_stats_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<CheeseNode>());
    rclcpp::shutdown();
    return 0;
}

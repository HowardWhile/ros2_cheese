#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iomanip>
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
#include "std_srvs/srv/trigger.hpp"

namespace
{
constexpr auto kTopicProbePeriod = std::chrono::milliseconds(1000);
constexpr char kRawImageType[] = "sensor_msgs/msg/Image";
constexpr char kCompressedImageType[] = "sensor_msgs/msg/CompressedImage";
const std::filesystem::path kDefaultCaptureDir{"/tmp/fiibot_cheese"};

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
        max_bytes_ = declare_parameter<int64_t>("max_bytes", 1024LL * 1024LL * 1024LL);

        if (max_files_ < 0)
        {
            RCLCPP_WARN(get_logger(), "max_files must be >= 0; using unlimited file count");
            max_files_ = 0;
        }
        if (max_bytes_ < 0)
        {
            RCLCPP_WARN(get_logger(), "max_bytes must be >= 0; using unlimited storage size");
            max_bytes_ = 0;
        }

        std::filesystem::create_directories(capture_dir_);

        trigger_srv_ = create_service<std_srvs::srv::Trigger>(
            "trigger", std::bind(&CheeseNode::handleTrigger, this, std::placeholders::_1, std::placeholders::_2));

        topic_probe_timer_ = create_wall_timer(kTopicProbePeriod, std::bind(&CheeseNode::probeImageTopic, this));
        probeImageTopic();
        pruneCaptures();

        RCLCPP_INFO(get_logger(), "Cheese service: ~/trigger");
        RCLCPP_INFO(get_logger(), "Cheese image topic: %s", sanitize_topic_for_log(image_topic_).c_str());
        RCLCPP_INFO(get_logger(), "Cheese capture dir: %s", capture_dir_.string().c_str());
        RCLCPP_INFO(get_logger(), "Cheese limits: max_files=%d, max_bytes=%ld", max_files_,
                    static_cast<long>(max_bytes_));
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
        }
        catch (const std::exception &exc)
        {
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
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "Failed to decode compressed image");
            return;
        }

        updateLatestImage(decoded_image);
    }

    void updateLatestImage(const cv::Mat &image)
    {
        std::lock_guard<std::mutex> lock(image_mutex_);
        latest_image_ = image.clone();
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
    int64_t max_bytes_ = 1024LL * 1024LL * 1024LL;
    ImageTopicKind subscribed_kind_ = ImageTopicKind::kUnknown;
    cv::Mat latest_image_;
    std::mutex image_mutex_;
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr raw_sub_;
    rclcpp::Subscription<sensor_msgs::msg::CompressedImage>::SharedPtr compressed_sub_;
    rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr trigger_srv_;
    rclcpp::TimerBase::SharedPtr topic_probe_timer_;
};

int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<CheeseNode>());
    rclcpp::shutdown();
    return 0;
}

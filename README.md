# ros2_cheese

**English** | [繁體中文](README_zh.md)

A trigger-based image capture package for ROS 2.

## Packages

| Package | Description |
| --- | --- |
| `cheese` | Trigger-based image capture and storage |
| `cheese_interfaces` | Custom service interfaces for Cheese |

## Nodes

| Node | Description |
| --- | --- |
| `cheese` | Subscribes to an image topic and saves an image when triggered |

## Launch files

| Launch file | Description |
| --- | --- |
| `cheese.launch.py` | Starts the Cheese image capture service |

## Usage

Start the Cheese node:

```shell
ros2 run cheese cheese
```

In another terminal, call the standard ROS 2 service to capture an image:

```shell
ros2 service call /cheese/trigger std_srvs/srv/Trigger "{}"
```

You can also provide a string to use as the filename suffix:

```shell
ros2 service call /cheese/string_trigger cheese_interfaces/srv/StringTrigger "{message: test}"
```

### Advanced usage

Start the Cheese node with its launch file:

```shell
ros2 launch cheese cheese.launch.py
```

The default output directory is:

```shell
/tmp/ros2_cheese
```

Specify an image topic:

```shell
ros2 launch cheese cheese.launch.py image_topic:=/camera/color/image_raw
```

Specify an output directory:

```shell
ros2 launch cheese cheese.launch.py capture_dir:=/tmp/ros2_cheese
```

Specify a node name:

```shell
ros2 launch cheese cheese.launch.py node_name:=camera_cheese
```

Specify a namespace:

```shell
ros2 launch cheese cheese.launch.py namespace:=camera node_name:=cheese
```

Set file count and storage limits:

```shell
ros2 launch cheese cheese.launch.py max_files:=1000 max_mb:=1024
```

## Services

| Service | Type | Description |
| --- | --- | --- |
| `/cheese/trigger` | `std_srvs/srv/Trigger` | Captures and saves an image |
| `/cheese/string_trigger` | `cheese_interfaces/srv/StringTrigger` | Captures an image and uses `message` as the filename suffix |

## Topics

| Topic | Type | Description |
| --- | --- | --- |
| `/cheese/status` | `std_msgs/msg/String` | Publishes image stream status once per second |

The `/cheese/status` payload is a JSON string containing the subscription
state, stream health, frame rate, bandwidth, and output directory statistics.

Example status payload:

```json
{
  "image_topic": "/camera/color/image_raw/compressed",
  "subscribed": true,
  "subscription_type": "compressed",
  "stream_ok": true,
  "stream_abnormal": false,
  "seconds_since_last_image": 0.012,
  "window": {
    "seconds": 1.0,
    "frames": 20,
    "bytes": 7123456,
    "failures": 0
  },
  "total_failures": 0,
  "stats_window_sec": 5.0,
  "fps": {
    "current": 20.0,
    "min": 19.8,
    "avg": 20.1,
    "max": 20.4,
    "samples": 100
  },
  "bandwidth_mbps": {
    "current": 57.0,
    "min": 55.8,
    "avg": 56.5,
    "max": 58.1,
    "samples": 100
  },
  "capture_dir": {
    "path": "/tmp/ros2_cheese",
    "exists": true,
    "files": 12,
    "bytes": 3456789,
    "mb": 3.3,
    "max_files": 1000,
    "max_mb": 1024,
    "max_bytes": 1073741824
  }
}
```

### Status fields

| Field | Description |
| --- | --- |
| `image_topic` | Configured image topic |
| `subscribed` | Whether the node has subscribed successfully |
| `subscription_type` | Subscription type: `raw`, `compressed`, or `unknown` |
| `stream_ok` | Whether the image stream is healthy |
| `stream_abnormal` | Whether the image stream is abnormal |
| `seconds_since_last_image` | Seconds since the last valid image, or `-1` if no image has been received |
| `window` | Statistics for the latest status interval, approximately one second |
| `window.frames` | Images received during the latest status interval |
| `window.bytes` | Image data received during the latest status interval |
| `window.failures` | Image processing failures during the latest status interval |
| `total_failures` | Total image processing failures since startup |
| `stats_window_sec` | Sliding-window duration for FPS and bandwidth statistics |
| `fps.current` | Average FPS during the latest status interval |
| `fps.min` / `fps.avg` / `fps.max` | Minimum, average, and maximum callback FPS within the statistics window |
| `fps.samples` | Number of FPS samples in the statistics window |
| `bandwidth_mbps.current` | Average bandwidth during the latest status interval, in Mbps |
| `bandwidth_mbps.min` / `bandwidth_mbps.avg` / `bandwidth_mbps.max` | Minimum, average, and maximum callback bandwidth within the statistics window |
| `bandwidth_mbps.samples` | Number of bandwidth samples in the statistics window |
| `capture_dir.path` | Image output directory |
| `capture_dir.exists` | Whether the output directory exists |
| `capture_dir.files` | Number of files in the output directory |
| `capture_dir.bytes` | Output directory size in bytes |
| `capture_dir.mb` | Output directory size in MB |
| `capture_dir.max_files` | File retention limit; `0` means unlimited |
| `capture_dir.max_mb` | Storage limit in MB; `0` means unlimited |
| `capture_dir.max_bytes` | Byte representation of the `max_mb` limit |

## Output

Successful captures are saved as `.jpg` files. The suffix is omitted when
`message` is empty:

```text
/tmp/ros2_cheese/YYYYMMDD-HHMMSS-mmm-<message>.jpg
```

The service request's `message` is used as the filename suffix. Characters
that are unsuitable for filenames are replaced with `_`. The response's
`message` contains the full path of the saved image.

## Notes

`image_topic` supports these ROS 2 message types:

```text
sensor_msgs/msg/Image
sensor_msgs/msg/CompressedImage
```

Set `max_files` or `max_mb` to `0` to disable the corresponding limit.

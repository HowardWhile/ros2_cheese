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

The default output directory is:

```shell
/tmp/ros2_cheese
```

### Advanced usage

Start the Cheese node with its launch file:

```shell
ros2 launch cheese cheese.launch.py
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
| `/cheese/string_trigger` | [`cheese_interfaces/srv/StringTrigger`](cheese_interfaces/srv/StringTrigger.srv) | Captures an image and uses `message` as the filename suffix |

## Topics

| Topic | Type | Description |
| --- | --- | --- |
| [`/cheese/status`](doc/status.md#english) | `std_msgs/msg/String` | Publishes image stream status once per second |

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

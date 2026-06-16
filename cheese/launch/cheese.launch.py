from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    image_topic = LaunchConfiguration("image_topic")
    capture_dir = LaunchConfiguration("capture_dir")
    max_files = LaunchConfiguration("max_files")
    max_bytes = LaunchConfiguration("max_bytes")

    return LaunchDescription(
        [
            DeclareLaunchArgument("image_topic", default_value="/camera/color/image_raw"),
            DeclareLaunchArgument("capture_dir", default_value="/tmp/fiibot_cheese"),
            DeclareLaunchArgument("max_files", default_value="1000"),
            DeclareLaunchArgument("max_bytes", default_value="1073741824"),
            Node(
                package="cheese",
                executable="cheese",
                name="cheese",
                output="screen",
                parameters=[
                    {
                        "image_topic": image_topic,
                        "capture_dir": capture_dir,
                        "max_files": ParameterValue(max_files, value_type=int),
                        "max_bytes": ParameterValue(max_bytes, value_type=int),
                    }
                ],
            ),
        ]
    )

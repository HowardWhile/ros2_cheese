# -----------------------------------------------------------------------------
# Launch file for Cheese image capture node
#
# Author: Howard Cheng
# -----------------------------------------------------------------------------

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():

    # ---------------------------------------------------------------
    # 1. Launch arguments
    # ---------------------------------------------------------------
    # Image source topic; both raw and compressed images are supported.
    arg_image_topic = DeclareLaunchArgument(
        "image_topic",
        default_value="/camera/color/image_raw",
        description="Image topic to capture from",
    )

    # Node name
    arg_node_name = DeclareLaunchArgument(
        "node_name",
        default_value="cheese",
        description="Node name for the cheese image capture node",
    )

    # Node namespace; an empty string uses the root namespace.
    arg_namespace = DeclareLaunchArgument(
        "namespace",
        default_value="",
        description="Node namespace for the cheese image capture node",
    )

    # Directory where captured images are stored.
    arg_capture_dir = DeclareLaunchArgument(
        "capture_dir",
        default_value="/tmp/ros2_cheese",
        description="Directory used to save captured images",
    )

    # Maximum number of images to retain; 0 means unlimited.
    arg_max_files = DeclareLaunchArgument(
        "max_files",
        default_value="1000",
        description="Maximum number of captured images to keep; 0 means unlimited",
    )

    # Maximum image storage size in MB; 0 means unlimited.
    arg_max_mb = DeclareLaunchArgument(
        "max_mb",
        default_value="1024",
        description="Maximum storage size for captured images in MB; 0 means unlimited",
    )

    # Resolve launch configurations.
    image_topic = LaunchConfiguration("image_topic")
    node_name = LaunchConfiguration("node_name")
    namespace = LaunchConfiguration("namespace")
    capture_dir = LaunchConfiguration("capture_dir")
    max_files = ParameterValue(
        LaunchConfiguration("max_files"), value_type=int
    )
    max_mb = ParameterValue(
        LaunchConfiguration("max_mb"), value_type=int
    )

    # ---------------------------------------------------------------
    # 2. Cheese Node
    # ---------------------------------------------------------------
    cheese_node = Node(
        package="cheese",
        executable="cheese",
        name=node_name,
        namespace=namespace,
        output="screen",
        parameters=[
            {"image_topic": image_topic},
            {"capture_dir": capture_dir},
            {"max_files": max_files},
            {"max_mb": max_mb},
        ],
    )

    # ---------------------------------------------------------------
    # 3. Create Launch Description
    # ---------------------------------------------------------------
    ld = LaunchDescription()

    # Add all launch argument declarations.
    ld.add_action(arg_image_topic)
    ld.add_action(arg_node_name)
    ld.add_action(arg_namespace)
    ld.add_action(arg_capture_dir)
    ld.add_action(arg_max_files)
    ld.add_action(arg_max_mb)

    # Start the Cheese node.
    ld.add_action(cheese_node)

    return ld

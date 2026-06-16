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
    # 1. Launch Arguments (外部參數輸入)
    # ---------------------------------------------------------------
    # 影像來源 topic，可支援 raw image 或 compressed image
    arg_image_topic = DeclareLaunchArgument(
        "image_topic",
        default_value="/camera/color/image_raw",
        description="Image topic to capture from",
    )

    # Node 名稱
    arg_node_name = DeclareLaunchArgument(
        "node_name",
        default_value="cheese",
        description="Node name for the cheese image capture node",
    )

    # Node namespace，空字串表示使用 root namespace
    arg_namespace = DeclareLaunchArgument(
        "namespace",
        default_value="",
        description="Node namespace for the cheese image capture node",
    )

    # 儲存拍照結果的資料夾
    arg_capture_dir = DeclareLaunchArgument(
        "capture_dir",
        default_value="/tmp/ros2_cheese",
        description="Directory used to save captured images",
    )

    # 最多保留的影像數量，0 表示不限制
    arg_max_files = DeclareLaunchArgument(
        "max_files",
        default_value="1000",
        description="Maximum number of captured images to keep; 0 means unlimited",
    )

    # 最多保留的影像容量，單位 MB，0 表示不限制
    arg_max_mb = DeclareLaunchArgument(
        "max_mb",
        default_value="1024",
        description="Maximum storage size for captured images in MB; 0 means unlimited",
    )

    # 取得參數配置
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

    # 加入所有參數宣告
    ld.add_action(arg_image_topic)
    ld.add_action(arg_node_name)
    ld.add_action(arg_namespace)
    ld.add_action(arg_capture_dir)
    ld.add_action(arg_max_files)
    ld.add_action(arg_max_mb)

    # 啟動 cheese 節點
    ld.add_action(cheese_node)

    return ld

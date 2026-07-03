# Copyright 2026 Howard
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.


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
        'image_topic',
        default_value='/camera/color/image_raw',
        description='Image topic to capture from',
    )

    # Node name
    arg_node_name = DeclareLaunchArgument(
        'node_name',
        default_value='cheese',
        description='Node name for the cheese image capture node',
    )

    # Node namespace; an empty string uses the root namespace.
    arg_namespace = DeclareLaunchArgument(
        'namespace',
        default_value='',
        description='Node namespace for the cheese image capture node',
    )

    # Directory where captured images are stored.
    arg_capture_dir = DeclareLaunchArgument(
        'capture_dir',
        default_value='/tmp/ros2_cheese',
        description='Directory used to save captured images',
    )

    # Maximum number of images to retain; 0 means unlimited.
    arg_max_files = DeclareLaunchArgument(
        'max_files',
        default_value='1000',
        description='Maximum number of captured images to keep; 0 means unlimited',
    )

    # Maximum image storage size in MB; 0 means unlimited.
    arg_max_mb = DeclareLaunchArgument(
        'max_mb',
        default_value='1024',
        description='Maximum storage size for captured images in MB; 0 means unlimited',
    )

    # Resolve launch configurations.
    image_topic = LaunchConfiguration('image_topic')
    node_name = LaunchConfiguration('node_name')
    namespace = LaunchConfiguration('namespace')
    capture_dir = LaunchConfiguration('capture_dir')
    max_files = ParameterValue(
        LaunchConfiguration('max_files'), value_type=int
    )
    max_mb = ParameterValue(
        LaunchConfiguration('max_mb'), value_type=int
    )

    # ---------------------------------------------------------------
    # 2. Cheese Node
    # ---------------------------------------------------------------
    cheese_node = Node(
        package='cheese',
        executable='cheese',
        name=node_name,
        namespace=namespace,
        output='screen',
        parameters=[
            {'image_topic': image_topic},
            {'capture_dir': capture_dir},
            {'max_files': max_files},
            {'max_mb': max_mb},
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

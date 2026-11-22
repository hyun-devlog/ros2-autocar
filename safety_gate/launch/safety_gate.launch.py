import os 

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    config = os.path.join(
        get_package_share_directory('safety_gate'), 'config', 'safety_gate.yaml'
    )
    return LaunchDescription([
        Node(
            package='safety_gate',
            executable='safety_gate_node',
            name='safety_gate',
            parameters=[config],
            output='screen',
        ),
    ])
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import GroupAction, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import SetRemap


def include(package, *path):
    return IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory(package), *path)
        )
    )
    

def generate_launch_description():
    gated_controller = GroupAction([
        SetRemap(src='odom_publisher:/cmd_vel', dst='/safety_gate/cmd_vel'),
        SetRemap(
            src='odom_publisher:/controller/cmd_vel', dst='/safety_gate/controller/cmd_vel'
        ),
        include('controller', 'launch', 'controller.launch.py'),
    ])
    
    return LaunchDescription([
        include('health_monitor', 'launch', 'health_monitor.launch.py'),
        include('safety_gate', 'launch', 'safety_gate.launch.py'),
        gated_controller,
        include('peripherals', 'launch', 'lidar.launch.py'),
    ])
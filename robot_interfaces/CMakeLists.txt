cmake_minimum_required(VERSION 3.8)
project(robot_interfaces)

# 빌드 도구와 의존 패키지
find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)      # .msg -> C++/Python 코드 생성기
find_package(std_msgs REQUIRED)                       # RobotStatus의 std_msgs/Header

# .msg를 코드로 변환
# 메시지 추가 시 이 부분에 추가 필요
rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/FaultItem.msg"
  "msg/RobotStatus.msg"
  DEPENDENCIES std_msgs
)

# 실행에 필요한 라이브러리
ament_export_dependencies(rosidl_default_runtime)
ament_package()
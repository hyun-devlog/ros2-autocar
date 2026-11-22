cmake_minimum_required(VERSION 3.8)
project(safety_gate)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()


find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(robot_interfaces REQUIRED)

add_library(gate_core STATIC
  src/gate_logic.cpp
)

target_include_directories(gate_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

add_executable(safety_gate_node src/safety_gate_node.cpp)
target_link_libraries(safety_gate_node gate_core)
ament_target_dependencies(safety_gate_node rclcpp geometry_msgs robot_interfaces)

install(DIRECTORY include/ DESTINATION include)
install(TARGETS safety_gate_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_gate_logic test/test_gate_logic.cpp)
  target_link_libraries(test_gate_logic gate_core)
endif()

ament_package()

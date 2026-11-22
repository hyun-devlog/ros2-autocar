cmake_minimum_required(VERSION 3.8)
project(health_monitor)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)

add_library(health_core STATIC
  src/rate_tracker.cpp
  src/state_evaluator.cpp
  src/system_probe.cpp
)

target_include_directories(health_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

add_library(health_ros STATIC
  src/topic_watchdog.cpp
)

target_link_libraries(health_ros health_core)
ament_target_dependencies(health_ros rclcpp)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_rate_tracker test/test_rate_tracker.cpp)
  target_link_libraries(test_rate_tracker health_core)
  ament_add_gtest(test_state_evaluator test/test_state_evaluator.cpp)
  target_link_libraries(test_state_evaluator health_core)
  ament_add_gtest(test_system_probe test/test_system_probe.cpp)
  target_link_libraries(test_system_probe health_core)
endif()

install(DIRECTORY include/ DESTINATION include)

ament_package()
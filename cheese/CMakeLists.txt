cmake_minimum_required(VERSION 3.8)
project(cheese)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(cheese_interfaces REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)

add_executable(cheese src/cheese_node.cpp)
target_compile_features(cheese PUBLIC cxx_std_17)
ament_target_dependencies(
  cheese
  cheese_interfaces
  cv_bridge
  rclcpp
  sensor_msgs
  std_msgs
  std_srvs
)
target_link_libraries(cheese ${OpenCV_LIBS})

install(TARGETS cheese
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # Preserve the project's existing C++ style instead of enforcing ROS formatting.
  set(ament_cmake_cpplint_FOUND TRUE)
  set(ament_cmake_uncrustify_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()

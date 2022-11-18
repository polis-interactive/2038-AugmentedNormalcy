
cmake_minimum_required(VERSION 3.18)
project(GlfwTest)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall")

set(GLAD_DIR ./include/glad/)
include_directories(./include ${GLAD_DIR}/include)

add_executable(
  glfw_test
  main.cpp
  graphics.cpp
  camera.cpp
  ${GLAD_DIR}/src/glad.c
  ${GLAD_DIR}/src/glad_egl.c
)


find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBCAMERA REQUIRED libcamera)
message(STATUS "libcamera library found:")
message(STATUS "    version: ${LIBCAMERA_VERSION}")
message(STATUS "    libraries: ${LIBCAMERA_LINK_LIBRARIES}")
message(STATUS "    include path: ${LIBCAMERA_INCLUDE_DIRS}")
include_directories(${CMAKE_SOURCE_DIR} ${LIBCAMERA_INCLUDE_DIRS})

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(glfw_test PUBLIC pthread glfw3 EGL GL dl ${LIBCAMERA_LINK_LIBRARIES})

cmake_minimum_required(VERSION 3.23)
project(TestImGui)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall")

set(IMGUI_DIR ./include/imgui/)
set(GLAD_DIR ./include/glad/)
include_directories(${IMGUI_DIR} ${IMGUI_DIR}/backends ${GLAD_DIR}/include)

add_executable(
  example_imgui
  main.cpp
  ${GLAD_DIR}/src/glad.c
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(example_imgui PUBLIC glfw /lib/aarch64-linux-gnu/libGLX_mesa.so.0 dl)

set_target_properties( example_imgui PROPERTIES COMPILE_FLAGS "-DIMGUI_IMPL_OPENGL_ES2" )


cmake_minimum_required(VERSION 3.16)

project(AugmentedNormalcy_NvJpegScratch LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -D_GLIBCXX_DEBUG -fsanitize=undefined -fno-omit-frame-pointer")

add_definitions(-DTMP_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(nv_jpeg_scratch main.cpp)

target_link_libraries(nv_jpeg_scratch jetson_mm pthread)


cmake_minimum_required(VERSION 3.16)

project(AugmentedNormalcy_BufferTest LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -D_GLIBCXX_DEBUG -fsanitize=undefined -fno-omit-frame-pointer")

add_definitions(-DTMP_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(buffer_test buffer_test.cpp)

target_link_libraries(buffer_test jetson_mm pthread)
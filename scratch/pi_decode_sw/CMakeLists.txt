

cmake_minimum_required(VERSION 3.16)

project(AugmentedNormalcy_PiDecodeSw LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -D_GLIBCXX_DEBUG -fsanitize=undefined -fno-omit-frame-pointer")

find_library(JPEG_LIBRARY jpeg REQUIRED)

add_definitions(-DTMP_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(pi_decode_sw main.cpp)

target_link_libraries(pi_decode_sw pthread jpeg)
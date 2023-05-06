

cmake_minimum_required(VERSION 3.16)

project(AugmentedNormalcy_PiEncodeSw LANGUAGES CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -D_GLIBCXX_DEBUG -fsanitize=undefined -fno-omit-frame-pointer")

pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavcodec
    libavdevice
    libavformat
    libswresample
)

add_definitions(-DTMP_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(pi_encode_av main.cpp)

target_link_libraries(pi_encode_av pthread ${LIBAV_LIBRARIES})
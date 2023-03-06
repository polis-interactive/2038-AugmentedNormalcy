
cmake_minimum_required(VERSION 3.18)

project(AugmentedNormalcy_JetsonEncoder LANGUAGES CXX)

set(
    SOURCES
        video_encode_csvparser.cpp
        video_encode_main.cpp
)

add_executable(jetson_encoder ${SOURCES})
target_link_libraries(jetson_encoder jetson_mm pthread)
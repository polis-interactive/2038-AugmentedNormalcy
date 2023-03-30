cmake_minimum_required(VERSION 3.16)

project(AugmentedNormalcy_ThirdParty_JetsonMM LANGUAGES CXX)

set(
    JETSON_MM_SOURCES
        src/NvApplicationProfiler.cpp
        src/NvBuffer.cpp
        src/NvBufSurface.cpp
        src/NvElement.cpp
        src/NvElementProfiler.cpp
        src/NvJpegEncoder.cpp
        src/NvLogging.cpp
        src/NvUtils.cpp
        src/NvV4l2Element.cpp
        src/NvV4l2ElementPlane.cpp
        src/NvVideoDecoder.cpp
        src/NvVideoEncoder.cpp
)

add_library(jetson_mm STATIC ${JETSON_MM_SOURCES})
target_link_libraries(jetson_mm PUBLIC cuda cudart nvjpeg v4l2 nvbufsurface nvbufsurftransform nvosd)
target_include_directories(jetson_mm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
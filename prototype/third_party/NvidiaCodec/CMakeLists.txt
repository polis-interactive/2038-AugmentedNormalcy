# Copyright 2020-2022 NVIDIA Corporation.  All rights reserved.
#
# Please refer to the NVIDIA end user license agreement (EULA) associated
# with this source code for terms and conditions that govern your use of
# this software. Any use, reproduction, disclosure, or distribution of
# this software and related documentation outside the terms of the EULA
# is strictly prohibited.

# 3.7 is required for FindVulkan module support in CMake.
cmake_minimum_required(VERSION 3.7)

project(NvidiaCodec LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


if(WIN32)
    Set(CUVID_LIB NvLib/nvcuvid.lib)
    set(NVENCODEAPI_LIB NvLib/nvencodeapi.lib)
else()
    find_library(CUVID_LIB nvcuvid)
    find_library(NVENCODEAPI_LIB nvidia-encode)
endif()


set(NV_CODEC_SOURCES
    NvEncoder/NvEncoder.cpp
    NvDecoder/NvDecoder.cpp
    NvEncoder/NvEncoderCuda.cpp
)

set(NV_CODEC_CUDA_UTILS
    NvUtils/ColorSpace.cu
    NvUtils/BitDepth.cu
)

set(NV_CODEC_HDRS
    NvEncoder/NvEncoder.h
    NvDecoder/NvDecoder.h
    NvEncoder/NvEncoderCuda.h
    NvInterface/nvEncodeAPI.h
    NvUtils/NvCodecUtils.h
    NvInterface/cuviddec.h
    NvInterface/nvcuvid.h
    NvUtils/ColorSpace.h
)

source_group( "headers" FILES ${NV_CODEC_HDRS})
source_group( "sources" FILES ${NV_CODEC_SOURCES} ${NV_CODEC_CUDA_UTILS})

set(CUDA_HOST_COMPILER ${CMAKE_CXX_COMPILER})

if(NOT "${CUDA_NVCC_FLAGS}" MATCHES "-std=c\\+\\+17" )
    list(APPEND CUDA_NVCC_FLAGS -std=c++17)
endif()

include_directories(${CUDA_INCLUDE_DIR})
add_library(${PROJECT_NAME} ${NV_CODEC_SOURCES} ${NV_CODEC_CUDA_UTILS} ${NV_CODEC_HDRS})
set_target_properties(${PROJECT_NAME} PROPERTIES CUDA_ARCHITECTURES ${CUDA_ARCHITECTURE_SETTING})
set_target_properties(${PROJECT_NAME} PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_library(CUDART_LIB cudart HINTS ${CUDA_TOOLKIT_ROOT_DIR}/lib64)
target_link_libraries(${PROJECT_NAME} ${CUDA_CUDA_LIBRARY} ${CUDART_LIB} ${CMAKE_DL_LIBS} ${NVENCODEAPI_LIB} ${CUVID_LIB})

set(NvidiaCodecLib ${PROJECT_NAME})
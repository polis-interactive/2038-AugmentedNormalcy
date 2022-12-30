# Copyright 2020-2022 NVIDIA Corporation.  All rights reserved.
#
# Please refer to the NVIDIA end user license agreement (EULA) associated
# with this source code for terms and conditions that govern your use of
# this software. Any use, reproduction, disclosure, or distribution of
# this software and related documentation outside the terms of the EULA
# is strictly prohibited.

# 3.7 is required for FindVulkan module support in CMake.
cmake_minimum_required(VERSION 3.7)

project(NvidiaCodec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NVCODEC_PUBLIC_INTERFACE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Interface)
set(NVCODEC_UTILS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Utils)
set(NV_ENC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/NvEncoder)
set(NV_DEC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/NvDecoder)


find_library(CUVID_LIB nvcuvid)
find_library(NVENCODEAPI_LIB nvidia-encode)


set(NV_CODEC_SOURCES
    ${NV_ENC_DIR}/NvEncoder.cpp
    ${NV_DEC_DIR}/NvDecoder.cpp
    ${NV_ENC_DIR}/NvEncoderCuda.cpp
)

set(NV_CODEC_CUDA_UTILS
    ${NVCODEC_UTILS_DIR}/ColorSpace.cu
    ${NVCODEC_UTILS_DIR}/BitDepth.cu
)

set(NV_CODEC_HDRS
    ${NV_ENC_DIR}/NvEncoder.h
    ${NV_DEC_DIR}/NvDecoder.h
    ${NV_ENC_DIR}/NvEncoderCuda.h
    ${NVCODEC_PUBLIC_INTERFACE_DIR}/nvEncodeAPI.h
    ${NVCODEC_UTILS_DIR}/NvCodecUtils.h
    ${NVCODEC_UTILS_DIR}/NvEncoderCLIOptions.h
    ${NVCODEC_PUBLIC_INTERFACE_DIR}/cuviddec.h
    ${NVCODEC_PUBLIC_INTERFACE_DIR}/nvcuvid.h
    ${NVCODEC_UTILS_DIR}/ColorSpace.h
)

source_group( "headers" FILES ${NV_ENC_HDRS} )
source_group( "sources" FILES ${APP_SOURCES} ${NV_ENC_SOURCES} ${NV_ENC_CUDA_UTILS})

set(CUDA_HOST_COMPILER ${CMAKE_CXX_COMPILER})

if(NOT "${CUDA_NVCC_FLAGS}" MATCHES "-std=c\\+\\+20" )
    list(APPEND CUDA_NVCC_FLAGS -std=c++20)
endif()

add_library(${PROJECT_NAME} STATIC ${NV_CODEC_SOURCES} ${NV_CODEC_CUDA_UTILS} ${NV_CODEC_HDRS})
set_target_properties(${PROJECT_NAME} PROPERTIES CUDA_SEPARABLE_COMPILATION ON)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_library(CUDART_LIB cudart HINTS ${CUDA_TOOLKIT_ROOT_DIR}/lib64)
target_link_libraries(${PROJECT_NAME} ${CUDART_LIB} ${CMAKE_DL_LIBS} ${NVENCODEAPI_LIB} ${CUVID_LIB})

set(NvidiaCodecLib ${PROJECT_NAME})
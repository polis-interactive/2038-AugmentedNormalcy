# Copyright 2020-2022 NVIDIA Corporation.  All rights reserved.
#
# Please refer to the NVIDIA end user license agreement (EULA) associated
# with this source code for terms and conditions that govern your use of
# this software. Any use, reproduction, disclosure, or distribution of
# this software and related documentation outside the terms of the EULA
# is strictly prohibited.

# 3.7 is required for FindVulkan module support in CMake.
cmake_minimum_required(VERSION 3.7)

project(scratch-video_sdk)

# Set C++11 for all projects and disable non-standard extensions
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_INSTALL_PREFIX .)

set(NVCODEC_PUBLIC_INTERFACE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Interface)
set(NVCODEC_UTILS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Utils)
set(NV_CODEC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/NvCodec)
set(NV_ENC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/NvCodec/NvEncoder)
set(NV_DEC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/NvCodec/NvDecoder)

set(NVCODEC_SAMPLES_INSTALL_DIR ${CMAKE_BINARY_DIR})

find_library(CUVID_LIB nvcuvid)
find_library(NVENCODEAPI_LIB nvidia-encode)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PC_AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(PC_AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(PC_AVUTIL REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(PC_SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)

set(NV_FFMPEG_HDRS ${PC_AVCODEC_INCLUDE_DIRS})
find_library(AVCODEC_LIBRARY NAMES avcodec
        HINTS
        ${PC_AVCODEC_LIBDIR}
        ${PC_AVCODEC_LIBRARY_DIRS}
        )
find_library(AVFORMAT_LIBRARY NAMES avformat
        HINTS
        ${PC_AVFORMAT_LIBDIR}
        ${PC_AVFORMAT_LIBRARY_DIRS}
        )
find_library(AVUTIL_LIBRARY NAMES avutil
        HINTS
        ${PC_AVUTIL_LIBDIR}
        ${PC_AVUTIL_LIBRARY_DIRS}
        )
find_library(SWRESAMPLE_LIBRARY NAMES swresample
        HINTS
        ${PC_SWRESAMPLE_LIBDIR}
        ${PC_SWRESAMPLE_LIBRARY_DIRS}
        )
set(AVCODEC_LIB ${AVCODEC_LIBRARY})
set(AVFORMAT_LIB ${AVFORMAT_LIBRARY})
set(AVUTIL_LIB ${AVUTIL_LIBRARY})
set(SWRESAMPLE_LIB ${SWRESAMPLE_LIBRARY})



add_subdirectory(example/local_app_trans_dec)



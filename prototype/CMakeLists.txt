cmake_minimum_required(VERSION 3.18)

project (
    AugmentedNormalcy_Prototype
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_VERBOSE_MAKEFILE ON)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(
        FATAL_ERROR
        "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
    )
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall")

set(root_dir ${CMAKE_CURRENT_LIST_DIR})
set(internal_dir ${root_dir}/internal)
set(third_party_dir ${root_dir}/third_party)

include(cmake/platform.cmake)
include(cmake/codec.cmake)
include(cmake/camera.cmake)

include_directories(${internal_dir})
add_subdirectory(${internal_dir}/infrastructure/codec)
add_subdirectory(${internal_dir}/infrastructure/camera)


add_subdirectory(${root_dir}/test)

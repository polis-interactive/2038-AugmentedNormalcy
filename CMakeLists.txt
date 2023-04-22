cmake_minimum_required(VERSION 3.16)

set(CMAKE_VERBOSE_MAKEFILE ON)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(
        FATAL_ERROR
        "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
    )
endif()

if (CMAKE_BUILD_TYPE STREQUAL Debug)
    message("Setting debug flags")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall")

SET(AN_PLATFORM "BROOSE_LINUX_LAPTOP" CACHE STRING "Sets up the build env")

# pre-project cmake files
include(cmake/platform.cmake)
include(cmake/toolchain.cmake)
include(cmake/features.cmake)

project (
    AugmentedNormalcy
    VERSION 1.0
    LANGUAGES CXX
)

# post-project cmake files
include(cmake/boost.cmake)
include(cmake/camera.cmake)
include(cmake/encoder.cmake)
include(cmake/decoder.cmake)
include(cmake/display.cmake)
include(cmake/location.cmake)

set(root_dir ${CMAKE_CURRENT_LIST_DIR})
set(internal_dir ${root_dir}/internal)
set(third_party_dir ${root_dir}/third_party)

include_directories(${root_dir}/internal)

add_subdirectory(${internal_dir}/infrastructure/tcp)
if (FEATURE_CAMERA)
    add_subdirectory(${internal_dir}/infrastructure/camera)
endif()
if (FEATURE_ENCODER)
    add_subdirectory(${third_party_dir}/jetson_multimedia_api)
    include_directories(${third_party_dir}/jetson_multimedia_api/include)
    add_subdirectory(${internal_dir}/infrastructure/encoder)
endif()
if (FEATURE_DECODER)
    add_subdirectory(${internal_dir}/infrastructure/decoder)
endif()
if (FEATURE_DISPLAY)
    set(glad_dir ${third_party_dir}/glad)
    include_directories(${glad_dir}/include)
    # for some reason, can only get this to build if I directly include glad instead of compiling it as a shared lib
    add_subdirectory(${internal_dir}/infrastructure/graphics)
endif()
if (FEATURE_LOCATION)
    include_directories(${third_party_dir}/libserialport)
    add_subdirectory(${internal_dir}/infrastructure/location)
endif()


add_subdirectory(${internal_dir}/service)

add_subdirectory(test)


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

include(cmake/platform.cmake)
include(cmake/toolchain.cmake)
include(cmake/boost.cmake)
include(cmake/camera.cmake)

project (
    AugmentedNormalcy
    VERSION 1.0
    LANGUAGES CXX
)

set(root_dir ${CMAKE_CURRENT_LIST_DIR})
set(internal_dir ${root_dir}/internal)
set(third_party_dir ${root_dir}/third_party)

include_directories(${root_dir}/internal)

add_subdirectory(${internal_dir}/infrastructure/tcp)
add_subdirectory(${internal_dir}/infrastructure/camera)

add_subdirectory(test)
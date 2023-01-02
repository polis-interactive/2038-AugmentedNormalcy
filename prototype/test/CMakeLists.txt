
cmake_minimum_required(VERSION 3.22)

project(AugmentedNormalcy_Prototype_Tests LANGUAGES CXX)

include_directories(${root_dir}/third_party/doctest)

set(
    tests
    ${CMAKE_CURRENT_SOURCE_DIR}/test_utility/buffer_pool.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_utility/worker_thread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_infrastructure/common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_infrastructure/udp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_infrastructure/bsp_packet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test_infrastructure/codec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
add_executable(${PROJECT_NAME} ${tests})
target_link_libraries(${PROJECT_NAME} ${source_library})
target_include_directories(${PROJECT_NAME} PRIVATE ${source_path})

target_include_directories(${PROJECT_NAME}
        PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/include
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>)
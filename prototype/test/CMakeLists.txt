
cmake_minimum_required(VERSION 3.18)

project(AugmentedNormalcy_Prototype_Tests LANGUAGES CXX)

include_directories(${root_dir}/third_party/doctest)

# ${CMAKE_CURRENT_SOURCE_DIR}/test_utility/buffer_pool.cpp
# ${CMAKE_CURRENT_SOURCE_DIR}/test_utility/worker_thread.cpp
# ${CMAKE_CURRENT_SOURCE_DIR}/test_infrastructure/common.cpp
# ${CMAKE_CURRENT_SOURCE_DIR}/test_infrastructure/udp.cpp
# ${CMAKE_CURRENT_SOURCE_DIR}/test_infrastructure/bsp_packet.cpp

set(
    tests
    test_infrastructure/test_camera/camera.cpp
    test_infrastructure/test_codec/codec.cpp
    main.cpp
)
add_definitions(-DTEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(tests ${tests})

if (AN_PLATFORM STREQUAL RPI)
    target_link_libraries(tests pthread)
endif()
target_link_libraries(tests camera)
target_link_libraries(tests codec)

target_include_directories(tests PRIVATE ${internal_dir})


cmake_minimum_required(VERSION 3.16)

project(AugmentedNormalcy_Prototype_Tests LANGUAGES CXX)

include_directories(${root_dir}/third_party/doctest)
include_directories(${BOOST_INCLUDEDIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(
    tests
        main.cpp
        test_infrastructure/test_tcp/test_context.cpp
        test_infrastructure/test_tcp/test_communication.cpp
)

if (LIBCAMERA_AVAILABLE)
    set(tests ${tests} test_infrastructure/test_camera/frame_capture.cpp)
endif()

add_definitions(-DTEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(tests ${tests})

target_link_libraries(tests pthread tcp camera)
target_include_directories(tests PRIVATE ${internal_dir})

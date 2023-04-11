
cmake_minimum_required(VERSION 3.16)

project(AugmentedNormalcy_Tests LANGUAGES CXX)

include_directories(${root_dir}/third_party/doctest)
include_directories(${BOOST_INCLUDEDIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(
    tests
        main.cpp
        test_infrastructure/test_tcp/test_context.cpp
        test_infrastructure/test_tcp/test_communication.cpp
)

set(tests_link_libraries pthread tcp)

if (LIBCAMERA_AVAILABLE)
    set(tests ${tests} test_infrastructure/test_camera/test_frame_capture.cpp)
endif()

if (FEATURE_CAMERA)
    set(tests ${tests} test_service/test_camera_streamer.cpp)
    set(tests_link_libraries ${tests_link_libraries} camera service)
endif()

if (FEATURE_ENCODER)
    set(tests ${tests}
            test_infrastructure/test_encoder/test_jetson_encoder.cpp
            test_service/test_server_encoder.cpp
    )
    set(tests_link_libraries ${tests_link_libraries} encoder service)
endif()


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -D_GLIBCXX_DEBUG")
add_definitions(-DTEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(tests ${tests})

target_link_libraries(tests ${tests_link_libraries})
target_include_directories(tests PRIVATE ${internal_dir})

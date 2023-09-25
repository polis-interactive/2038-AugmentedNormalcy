
include_directories(${BOOST_INCLUDEDIR})

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${root_dir}/bin)

add_definitions(-DAPPLICATION_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

if (
    NOT AN_PLATFORM_TYPE STREQUAL RPI
)
    add_executable(app_server server.cpp runtime.cpp)
    target_link_libraries(app_server PRIVATE pthread service)
endif()

if (
    AN_PLATFORM STREQUAL RPI_CAMERA
        OR AN_PLATFORM STREQUAL RPI_CAMERA_CC
        OR AN_PLATFORM STREQUAL BROOSE_LINUX_LAPTOP
)
    add_executable(app_camera camera.cpp runtime.cpp)
    target_link_libraries(app_camera PRIVATE pthread service)
endif()

if (
    AN_PLATFORM STREQUAL RPI_HEADSET
        OR AN_PLATFORM STREQUAL RPI_HEADSET_CC
        OR AN_PLATFORM STREQUAL BROOSE_LINUX_LAPTOP
)
    add_executable(app_headset headset.cpp runtime.cpp)
    target_link_libraries(app_headset PRIVATE pthread service graphics decoder tcp)
endif()

if (
        AN_PLATFORM STREQUAL RPI_DISPLAY
        OR AN_PLATFORM STREQUAL RPI_DISPLAY_CC
        OR AN_PLATFORM STREQUAL BROOSE_LINUX_LAPTOP
)
    add_executable(app_display display.cpp runtime.cpp)
    target_link_libraries(app_display PRIVATE pthread service graphics decoder tcp)
endif()
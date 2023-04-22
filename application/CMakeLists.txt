
include_directories(${BOOST_INCLUDEDIR})

if (
    AN_PLATFORM_TYPE STREQUAL JETSON
)
    add_executable(app_server server.cpp)
    target_link_libraries(app_server PRIVATE pthread service)
endif()

if (
    AN_PLATFORM STREQUAL RPI_CAMERA
        OR AN_PLATFORM STREQUAL RPI_CAMERA_CC
)
    add_executable(app_camera camera.cpp)
    target_link_libraries(app_camera PRIVATE pthread service)
endif()

if (
    AN_PLATFORM STREQUAL RPI_HEADSET
        OR AN_PLATFORM STREQUAL RPI_HEADSET_CC
        OR AN_PLATFORM STREQUAL BROOSE_LINUX_LAPTOP
)
    add_executable(app_headset headset.cpp)
    target_link_libraries(app_headset PRIVATE pthread service graphics decoder tcp)
endif()
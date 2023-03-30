cmake_minimum_required(VERSION 3.18)

set(SOURCES camera.cpp fake_camera.cpp)
set(TARGET_LIBS)

if (
    AN_PLATFORM STREQUAL RPI_CAMERA OR
    AN_PLATFORM STREQUAL RPI_CAMERA_CC
)
    set(TARGET_LIBS ${TARGET_LIBS} pthread)
endif()

if (LIBCAMERA_AVAILABLE)
    set(SOURCES ${SOURCES} libcamera_camera.cpp)
    set(TARGET_LIBS ${TARGET_LIBS} ${LIBCAMERA_LINK_LIBRARIES})
endif()


add_library(camera STATIC ${SOURCES})
target_link_libraries(camera PRIVATE ${TARGET_LIBS})
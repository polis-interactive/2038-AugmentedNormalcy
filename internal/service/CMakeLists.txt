cmake_minimum_required(VERSION 3.18)

set(SOURCES camera_streamer.cpp)
set(TARGET_LIBS)

if (
    AN_PLATFORM STREQUAL RPI_CAMERA OR
    AN_PLATFORM STREQUAL RPI_CAMERA_CC
)
    set(TARGET_LIBS ${TARGET_LIBS} pthread)
endif()

add_library(service STATIC ${SOURCES})
target_link_libraries(service ${TARGET_LIBS})
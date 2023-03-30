cmake_minimum_required(VERSION 3.16)

set(SOURCES)
set(TARGET_LIBS)

if (
    AN_PLATFORM STREQUAL RPI_CAMERA OR
    AN_PLATFORM STREQUAL RPI_CAMERA_CC
)
    set(TARGET_LIBS ${TARGET_LIBS} pthread)
endif()

if (FEATURE_CAMERA)
    set(SOURCES ${SOURCES} camera_streamer.cpp)
# for now, putting this here so non camera services don't get boned
    add_library(service STATIC ${SOURCES})
    target_link_libraries(service PRIVATE ${TARGET_LIBS})
endif()
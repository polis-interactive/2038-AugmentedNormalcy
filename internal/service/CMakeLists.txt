cmake_minimum_required(VERSION 3.16)

set(SOURCES)
set(TARGET_LIBS pthread)

include_directories(${BOOST_INCLUDEDIR})

if (FEATURE_CAMERA)
    set(SOURCES ${SOURCES} camera_streamer.cpp)
    set(TARGET_LIBS ${TARGET_LIBS} camera)
endif()

if (FEATURE_ENCODER)
    set(SOURCES ${SOURCES} server_encoder.cpp)
    set(TARGET_LIBS ${TARGET_LIBS} encoder)
endif()

if (FEATURE_DISPLAY)
    set(SOURCES ${SOURCES} headset_streamer.cpp)
    set(TARGET_LIBS ${TARGET_LIBS} decoder graphics)
endif()

add_library(service STATIC ${SOURCES})
target_link_libraries(service PRIVATE ${TARGET_LIBS})

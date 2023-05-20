cmake_minimum_required(VERSION 3.16)

set(SOURCES)
set(TARGET_LIBS pthread tcp)

include_directories(${BOOST_INCLUDEDIR})

if (FEATURE_CAMERA)
    set(SOURCES ${SOURCES} camera_streamer.cpp)
    set(TARGET_LIBS ${TARGET_LIBS} camera encoder)
endif()

if (NOT AN_PLATFORM_TYPE STREQUAL RPI)
    set(SOURCES ${SOURCES} server/server_streamer.cpp server/connection_manager.cpp)
    set(TARGET_LIBS ${TARGET_LIBS})
endif()

if (FEATURE_DISPLAY)
    set(SOURCES ${SOURCES} headset_streamer.cpp)
    set(TARGET_LIBS ${TARGET_LIBS} decoder graphics)
endif()

add_library(service STATIC ${SOURCES})
target_link_libraries(service PRIVATE ${TARGET_LIBS})

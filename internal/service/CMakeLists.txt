cmake_minimum_required(VERSION 3.16)

set(SOURCES)
set(TARGET_LIBS pthread tcp websocket domain)

include_directories(${BOOST_INCLUDEDIR})

if (FEATURE_CAMERA)
    set(SOURCES ${SOURCES} camera_streamer.cpp)
    set(TARGET_LIBS ${TARGET_LIBS} camera encoder)
endif()

if (NOT AN_PLATFORM_TYPE STREQUAL RPI)
    set(SOURCES ${SOURCES} server/server_streamer.cpp server/connection_manager.cpp)
    set(TARGET_LIBS ${TARGET_LIBS})
endif()

if (FEATURE_GRAPHICS)
    if (
        (AN_PLATFORM STREQUAL RPI_HEADSET) OR (AN_PLATFORM STREQUAL RPI_HEADSET_CC)
    )
        set(SOURCES ${SOURCES} headset/headset_streamer.cpp)
        set(TARGET_LIBS ${TARGET_LIBS} decoder graphics websocket gpio)
    elseif(
        (AN_PLATFORM STREQUAL RPI_DISPLAY) OR (AN_PLATFORM STREQUAL RPI_DISPLAY_CC)
    )
        set(SOURCES ${SOURCES} display_streamer.cpp)
        set(TARGET_LIBS ${TARGET_LIBS} decoder graphics websocket)
    else()
        set(SOURCES ${SOURCES} headset/headset_streamer.cpp display_streamer.cpp)
        set(TARGET_LIBS ${TARGET_LIBS} decoder graphics websocket gpio)
    endif()
endif()

add_library(service STATIC ${SOURCES})
target_link_libraries(service PRIVATE ${TARGET_LIBS})

cmake_minimum_required(VERSION 3.16)

set(SOURCES)
set(TARGET_LIBS pthread)

include_directories(${BOOST_INCLUDEDIR})

if (FEATURE_CAMERA)
    set(SOURCES ${SOURCES} camera_streamer.cpp)
# for now, putting this here so non camera services don't get boned
    add_library(service STATIC ${SOURCES})
    target_link_libraries(service PRIVATE ${TARGET_LIBS} camera)
endif()

if (FEATURE_ENCODER)
    set(SOURCES ${SOURCES} server_encoder.cpp)
    # for now, putting this here so non camera services don't get boned
    add_library(service STATIC ${SOURCES})
    target_link_libraries(service PRIVATE ${TARGET_LIBS} encoder)
endif()


if (V4L2_DECODER_AVAILABLE)

    set(SOURCES v4l2_decoder.cpp)
    set(TARGET_LIBS pthread)

    add_definitions(-DTHIS_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

    add_library(decoder STATIC ${SOURCES})
    target_link_libraries(decoder PRIVATE ${TARGET_LIBS})

endif()


if (V4L2_DECODER_AVAILABLE)

    set(SOURCES v4l2_decoder.cpp)
    set(TARGET_LIBS pthread)

    add_library(decoder STATIC ${SOURCES})
    target_link_libraries(decoder PRIVATE ${TARGET_LIBS})

endif()

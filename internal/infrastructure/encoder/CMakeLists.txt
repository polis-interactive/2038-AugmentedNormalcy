

if (V4L2_ENCODER_AVAILABLE)

    set(SOURCES v4l2_encoder.cpp)
    set(TARGET_LIBS pthread)

    add_library(encoder STATIC ${SOURCES})
    target_link_libraries(encoder PRIVATE ${TARGET_LIBS})

endif()

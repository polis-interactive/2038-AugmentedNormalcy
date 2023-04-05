

if (JETSON_ENCODER_AVAILABLE)

    set(SOURCES jetson_encoder.cpp)
    set(TARGET_LIBS jetson_mm pthread)

    add_library(encoder STATIC ${SOURCES})
    target_link_libraries(encoder PRIVATE ${TARGET_LIBS})

endif()

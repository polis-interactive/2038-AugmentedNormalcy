
set(
    SOURCES graphics.cpp null_graphics.cpp display_graphics.cpp

)
set(TARGET_LIBS pthread)


if (HEADSET_GRAPHICS_AVAILABLE OR DISPLAY_GRAPHICS_AVAILABLE)
    set(SOURCES ${SOURCES} ${glad_dir}/src/glad.c ${glad_dir}/src/glad_egl.c)
    set(TARGET_LIBS ${TARGET_LIBS} ${GLFW3_LINK_LIBRARIES})
endif()

if (HEADSET_GRAPHICS_AVAILABLE)
    set(SOURCES ${SOURCES} headset_graphics.cpp)
endif ()

if (DISPLAY_GRAPHICS_AVAILABLE)
    set(SOURCES ${SOURCES} display_graphics.cpp)
endif ()

add_library(graphics ${SOURCES})
target_link_libraries(graphics PRIVATE ${TARGET_LIBS})
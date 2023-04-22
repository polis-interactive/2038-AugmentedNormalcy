
set(SOURCES graphics.cpp ${glad_dir}/src/glad.c ${glad_dir}/src/glad_egl.c)
set(TARGET_LIBS pthread ${GLFW3_LINK_LIBRARIES})

if(NOT AN_PLATFORM STREQUAL RPI)
    set(TARGET_LIBS ${TARGET_LIBS} X11)
endif()

add_library(graphics ${SOURCES})
target_link_libraries(graphics PRIVATE ${TARGET_LIBS})
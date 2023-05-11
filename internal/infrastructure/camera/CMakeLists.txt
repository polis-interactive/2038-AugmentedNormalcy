
set(SOURCES libcamera_camera.cpp)
set(TARGET_LIBS pthread ${LIBCAMERA_LINK_LIBRARIES})

add_library(camera STATIC ${SOURCES})
target_link_libraries(camera PRIVATE ${TARGET_LIBS})
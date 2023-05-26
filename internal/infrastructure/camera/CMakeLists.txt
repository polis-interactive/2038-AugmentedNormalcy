
set(SOURCES camera.cpp string_camera.cpp fake_camera.cpp)
set(TARGET_LIBS pthread)

if (LIBCAMERA_AVAILABLE)
    set(SOURCES ${SOURCES} libcamera_camera.cpp)
    include_directories(${LIBCAMERA_INCLUDE_DIRS})
    set(TARGET_LIBS ${TARGET_LIBS} ${LIBCAMERA_LINK_LIBRARIES})
endif()

add_library(camera STATIC ${SOURCES})
target_link_libraries(camera PRIVATE ${TARGET_LIBS})
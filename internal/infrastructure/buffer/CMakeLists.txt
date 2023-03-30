cmake_minimum_required(VERSION 3.18)

set(SOURCES buffer.cpp array_buffer.cpp)
set(TARGET_LIBS)

if (JETSON_BUFFER_AVAILABLE)
    set(SOURCES ${SOURCES} jetson_buffer.cpp)
    set(TARGET_LIBS ${TARGET_LIBS} jetson_mm)
endif()


add_library(buffer STATIC ${SOURCES})
target_link_libraries(buffer PRIVATE ${TARGET_LIBS})
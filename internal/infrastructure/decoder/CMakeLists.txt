
find_library(JPEG_LIBRARY jpeg REQUIRED)

set(SOURCES sw_decoder.cpp)
set(TARGET_LIBS pthread jpeg)

add_definitions(-DTHIS_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

add_library(decoder STATIC ${SOURCES})
target_link_libraries(decoder PRIVATE ${TARGET_LIBS})


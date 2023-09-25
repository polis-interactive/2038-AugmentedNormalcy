

find_library(JPEG_LIBRARY jpeg REQUIRED)

set(SOURCES encoder.cpp sw_encoder.cpp null_encoder.cpp)
set(TARGET_LIBS pthread jpeg)

add_library(encoder STATIC ${SOURCES})
target_link_libraries(encoder PRIVATE ${TARGET_LIBS})
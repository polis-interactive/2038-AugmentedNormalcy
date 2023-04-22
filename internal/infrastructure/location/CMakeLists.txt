

set(SOURCES location.cpp)
set(TARGET_LIBS ${LIBSERIAL_LINK_LIBRARIES} pthread)

add_library(location STATIC ${SOURCES})
include_directories(${LIBSERIAL_INCLUDE_DIRS}/libserial)
target_link_libraries(location PRIVATE ${TARGET_LIBS})
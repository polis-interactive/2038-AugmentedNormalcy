
set(SOURCES bms.cpp null_bms.cpp)
set(TARGET_LIBS pthread)

if (NOT BMS_SERIAL_AVAILABLE)
    set(SOURCES ${SOURCES} serial_bms.cpp)
endif()

add_library(bms STATIC ${SOURCES})
target_link_libraries(bms PRIVATE ${TARGET_LIBS})
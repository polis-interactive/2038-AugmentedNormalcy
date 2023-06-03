
set(SOURCES bms.cpp null_bms.cpp)
set(TARGET_LIBS pthread domain)

if (BMS_SERIAL_AVAILABLE)
    include_directories(${CppLinuxSerial_INCLUDE_DIRS})
    set(SOURCES ${SOURCES} serial_bms.cpp)
    set (TARGET_LIBS ${TARGET_LIBS} ${CppLinuxSerial_LIBRARIES})
endif()

add_library(bms STATIC ${SOURCES})
target_link_libraries(bms PRIVATE ${TARGET_LIBS})
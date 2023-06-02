
# technically, anyone with asio can use bms_serial... but it'll only work on the pi

if (
    FEATURE_BMS AND NOT (
        (AN_PLATFORM STREQUAL RPI_HEADSET) OR
        (AN_PLATFORM STREQUAL RPI_HEADSET_CC)
    )
)
    set(CppLinuxSerial_INCLUDE_DIRS /usr/local/include/)
    set(CppLinuxSerial_LIBRARIES /usr/local/lib/libCppLinuxSerial.a)
    find_package(CppLinuxSerial REQUIRED)
    message(STATUS "CppLinuxSerial library found:")
    message(STATUS "    include: ${CppLinuxSerial_INCLUDE_DIRS}")
    message(STATUS "    libraries: ${CppLinuxSerial_LIBRARIES}")
    set(BMS_SERIAL_AVAILABLE 1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_BMS_SERIAL_")
endif()
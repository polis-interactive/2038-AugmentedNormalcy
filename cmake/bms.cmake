
# technically, anyone with asio can use bms_serial... but it'll only work on the pi

if (
    FEATURE_BMS AND (
        (AN_PLATFORM STREQUAL RPI_HEADSET) OR
        (AN_PLATFORM STREQUAL RPI_HEADSET_CC)
    )
)
    set(BMS_SERIAL_AVAILABLE 1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_BMS_SERIAL_")
endif()

if (
    (AN_PLATFORM STREQUAL RPI_CAMERA) OR
        (AN_PLATFORM STREQUAL RPI_CAMERA_CC) OR
        (AN_PLATFORM STREQUAL BROOSE_LINUX_LAPTOP)
)
    set(FEATURE_CAMERA true)
    set(FEATURE_ENCODER true)
endif()

if (
    (AN_PLATFORM STREQUAL RPI_HEADSET) OR
        (AN_PLATFORM STREQUAL RPI_HEADSET_CC) OR
        (AN_PLATFORM STREQUAL BROOSE_LINUX_LAPTOP)
)
    set(FEATURE_DECODER true)
    set(FEATURE_GRAPHICS true)
    set(FEATURE_BMS true)
    set(FEATURE_GPIO true)
endif()

if (
    (AN_PLATFORM STREQUAL RPI_DISPLAY) OR
        (AN_PLATFORM STREQUAL RPI_DISPLAY_CC) OR
        (AN_PLATFORM STREQUAL BROOSE_LINUX_LAPTOP)
)
    set(FEATURE_DECODER true)
    set(FEATURE_GRAPHICS true)
endif()

if (NOT AN_PLATFORM_TYPE STREQUAL JETSON)
    # set(FEATURE_LOCATION true)
endif()
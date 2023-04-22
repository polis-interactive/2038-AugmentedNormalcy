
if (
    (AN_PLATFORM STREQUAL JETSON_CC) OR
        (AN_PLATFORM STREQUAL JETSON)
)
    set(FEATURE_ENCODER true)
endif()

if (
    (AN_PLATFORM STREQUAL RPI_CAMERA) OR
        (AN_PLATFORM STREQUAL RPI_CAMERA_CC) OR
        (AN_PLATFORM STREQUAL BROOSE_LINUX_LAPTOP)
)
    set(FEATURE_CAMERA true)
endif()

if (
    (AN_PLATFORM STREQUAL RPI_HEADSET) OR
        (AN_PLATFORM STREQUAL RPI_HEADSET_CC) OR
        (AN_PLATFORM STREQUAL BROOSE_LINUX_LAPTOP)
)
    set(FEATURE_DECODER true)
    set(FEATURE_DISPLAY true)
endif()
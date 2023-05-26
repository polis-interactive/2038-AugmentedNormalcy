
# this needs to filter rpi / broose linux to compile on windows with fake camera

if (
    FEATURE_GPIO AND (
        (AN_PLATFORM STREQUAL RPI_HEADSET) OR
        (AN_PLATFORM STREQUAL RPI_HEADSET_CC)
    )
)
    set(PIGPIO_AVAILABLE 1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_PIGIPO_")
endif()
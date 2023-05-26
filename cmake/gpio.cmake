
# this needs to filter rpi / broose linux to compile on windows with fake camera

if (
    FEATURE_GPIO AND (
        (AN_PLATFORM STREQUAL RPI_HEADSET) OR
        (AN_PLATFORM STREQUAL RPI_HEADSET_CC)
    )
)
    find_path(
            pigpio_INCLUDE_DIR
            NAMES pigpio.h pigpiod_if.h pigpiod_if2.h
            HINTS /usr/local/include
    )

    # Find the pigpio libraries.
    find_library(
            pigpio_LIBRARY
            NAMES libpigpio.so
            HINTS /usr/local/lib
    )
    find_library(
            pigpiod_if_LIBRARY
            NAMES libpigpiod_if.so
            HINTS /usr/local/lib
    )
    find_library(
            pigpiod_if2_LIBRARY
            NAMES libpigpiod_if2.so
            HINTS /usr/local/lib
    )

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(
            pigpio
            REQUIRED_VARS pigpio_INCLUDE_DIR pigpio_LIBRARY pigpiod_if_LIBRARY pigpiod_if2_LIBRARY
            DEFAULT_MSG
    )
    if(NOT pigpio_FOUND)
        message(FATAL_ERROR "pigpio not found")
    endif()

    set(PIGPIO_LINK_LIBRARIES pigpio_LIBRARY pigpiod_if_LIBRARY pigpiod_if2_LIBRARY)
    set(PIGPIO_INCLUDE_DIRS pigpio_INCLUDE_DIR)

    message(STATUS "pigpio library found:")
    message(STATUS "    libraries: ${PIGPIO_LINK_LIBRARIES}")
    message(STATUS "    include path: ${PIGPIO_INCLUDE_DIRS}")

    set(PIGPIO_AVAILABLE 1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_PIGIPO_")
endif()
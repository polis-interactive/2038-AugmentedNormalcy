
if (AN_PLATFORM STREQUAL JETSON_CC)
    set(BOOST_ROOT "${TARGET_ROOTFS}/usr/src/boost_1_81_0")

elseif(AN_PLATFORM STREQUAL JETSON)
    set(BOOST_ROOT "/usr/src/boost_1_81_0")

elseif(AN_PLATFORM STREQUAL BROOSE_LINUX_LAPTOP)
    set(BOOST_ROOT "/usr/local/lib/boost_1_81_0")

elseif(
    (AN_PLATFORM STREQUAL RPI_CAMERA) or
        (AN_PLATFORM STREQUAL RPI_HEADSET)
)
    set(BOOST_ROOT "/home/pi/build/boost_1_81_0")

elseif(
    (AN_PLATFORM STREQUAL RPI_CAMERA_CC) or
        (AN_PLATFORM STREQUAL RPI_HEADSET_CC)
)
    set(BOOST_ROOT "${TARGET_ROOTFS}/home/pi/build/boost_1_81_0")

endif()

set(BOOST_INCLUDEDIR  "${BOOST_ROOT}")
set(BOOST_LIBRARYDIR "${BOOST_ROOT}/stage/lib")
set(Boost_Version 1.81.0)

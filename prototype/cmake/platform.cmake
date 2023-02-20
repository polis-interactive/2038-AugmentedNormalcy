
if (WIN32)
    # assume broose windows; if we need, we can further disambiguate here
    set(AN_PLATFORM BROOSE_WINDOWS_LAPTOP)
    set(AN_PLATFORM_NUM 1)
elseif(EXISTS /opt/vc/include/bcm_host.h)
    # RPI; if we need, we can further disambiguate here (for instance, CM4 vs PI4B
    set(AN_PLATFORM RPI)
    set(AN_PLATFORM_NUM 20)
# why is this true for the pi? changed order to weed it out for now
elseif (NOT __arm__)
    # assume broose linux; if we need, we can further disambiguate here
    set(AN_PLATFORM BROOSE_LINUX_LAPTOP)
    set(AN_PLATFORM_NUM 10)
else()
    # jetson orion; if we need, we can further disambiguate here (for instance, CM4 vs PI4B
    set(AN_PLATFORM JETSON)
    set(AN_PLATFORM_NUM 30)
endif()

message("Using host: ${AN_PLATFORM}")
message("Using platform number: ${AN_PLATFORM_NUM}")

if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DPLATFORM_BROOSE_WINDOWS_LAPTOP=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DPLATFORM_BROOSE_LINUX_LAPTOP=10")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DPLATFORM_RPI=20")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DPLATFORM_JETSON=30")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_AN_PLATFORM_=${AN_PLATFORM_NUM}")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPLATFORM_BROOSE_WINDOWS_LAPTOP=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPLATFORM_BROOSE_LINUX_LAPTOP=10")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPLATFORM_RPI=20")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DPLATFORM_JETSON=30")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_AN_PLATFORM_=${AN_PLATFORM_NUM}")
endif()

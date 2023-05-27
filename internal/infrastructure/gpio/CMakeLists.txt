
set(SOURCES gpio.cpp null_gpio.cpp)
set(TARGET_LIBS)

if (PIGPIO_AVAILABLE)
    set(SOURCES ${SOURCES} pigpio_gpio.cpp)
    include_directories(${PIGPIO_INCLUDE_DIRS})
    set(TARGET_LIBS ${TARGET_LIBS} ${PIGPIO_LINK_LIBRARIES})
# else()
    # tmp for local development
    # include_directories(${third_party_dir}/pigpio)
    # set(SOURCES ${SOURCES} pigpio_gpio.cpp)
endif()

add_library(gpio STATIC ${SOURCES})
target_link_libraries(gpio PRIVATE ${TARGET_LIBS})
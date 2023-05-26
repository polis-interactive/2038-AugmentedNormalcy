
set(SOURCES gpio.cpp null_gpio.cpp)
set(TARGET_LIBS pthread)

if (NOT PIGPIO_AVAILABLE)
    set(SOURCES ${SOURCES} pigpio_gpio.cpp)
endif()

add_library(gpio STATIC ${SOURCES})
target_link_libraries(gpio PRIVATE ${TARGET_LIBS})
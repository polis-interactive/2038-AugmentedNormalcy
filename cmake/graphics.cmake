
if (FEATURE_GRAPHICS)
    set(GLFW3_VERSION 3.3)
    set(GLFW3_LINK_LIBRARIES glfw3 EGL GL dl)
    find_package(glfw3 ${GLFW3_VERSION} CONFIG REQUIRED)
    message(STATUS "glfw3 library found:")
    message(STATUS "    version: ${GLFW3_VERSION}")
    message(STATUS "    libraries: ${GLFW3_LINK_LIBRARIES}")
endif()


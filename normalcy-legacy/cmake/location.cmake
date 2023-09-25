
if (FEATURE_LOCATION)
    include(FindPkgConfig)
    pkg_check_modules(LIBSERIAL REQUIRED libserial)
    message(STATUS "libserial library found:")
    message(STATUS "    version: ${LIBSERIAL_VERSION}")
    message(STATUS "    include: ${LIBSERIAL_INCLUDE_DIRS}")
    message(STATUS "    libraries: ${LIBSERIAL_LINK_LIBRARIES}")
endif()

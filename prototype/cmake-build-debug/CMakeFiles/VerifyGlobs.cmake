# CMAKE generated file: DO NOT EDIT!
# Generated by CMake Version 3.23
cmake_policy(SET CMP0009 NEW)

# sources at cmd/CMakeLists.txt:10 (file)
file(GLOB NEW_GLOB LIST_DIRECTORIES true "/home/brucegoose/polis/2038-AugmentedNormalcy/prototype/cmd/asio/*.cpp")
set(OLD_GLOB
  "/home/brucegoose/polis/2038-AugmentedNormalcy/prototype/cmd/asio/udp_echo_server.cpp"
  )
if(NOT "${NEW_GLOB}" STREQUAL "${OLD_GLOB}")
  message("-- GLOB mismatch!")
  file(TOUCH_NOCREATE "/home/brucegoose/polis/2038-AugmentedNormalcy/prototype/cmake-build-debug/CMakeFiles/cmake.verify_globs")
endif()

# public_headers at CMakeLists.txt:42 (file)
file(GLOB_RECURSE NEW_GLOB LIST_DIRECTORIES false "/home/brucegoose/polis/2038-AugmentedNormalcy/prototype/include/*.hpp")
set(OLD_GLOB
  )
if(NOT "${NEW_GLOB}" STREQUAL "${OLD_GLOB}")
  message("-- GLOB mismatch!")
  file(TOUCH_NOCREATE "/home/brucegoose/polis/2038-AugmentedNormalcy/prototype/cmake-build-debug/CMakeFiles/cmake.verify_globs")
endif()

# internal_sources at CMakeLists.txt:44 (file)
file(GLOB_RECURSE NEW_GLOB LIST_DIRECTORIES false "/home/brucegoose/polis/2038-AugmentedNormalcy/prototype/internal/*.cpp")
set(OLD_GLOB
  )
if(NOT "${NEW_GLOB}" STREQUAL "${OLD_GLOB}")
  message("-- GLOB mismatch!")
  file(TOUCH_NOCREATE "/home/brucegoose/polis/2038-AugmentedNormalcy/prototype/cmake-build-debug/CMakeFiles/cmake.verify_globs")
endif()

# internal_headers at CMakeLists.txt:43 (file)
file(GLOB_RECURSE NEW_GLOB LIST_DIRECTORIES false "/home/brucegoose/polis/2038-AugmentedNormalcy/prototype/internal/*.hpp")
set(OLD_GLOB
  "/home/brucegoose/polis/2038-AugmentedNormalcy/prototype/internal/infrastructure/udp/context.hpp"
  )
if(NOT "${NEW_GLOB}" STREQUAL "${OLD_GLOB}")
  message("-- GLOB mismatch!")
  file(TOUCH_NOCREATE "/home/brucegoose/polis/2038-AugmentedNormalcy/prototype/cmake-build-debug/CMakeFiles/cmake.verify_globs")
endif()

# tests at test/CMakeLists.txt:8 (file)
file(GLOB_RECURSE NEW_GLOB LIST_DIRECTORIES false "/home/brucegoose/polis/2038-AugmentedNormalcy/prototype/test/*.cpp")
set(OLD_GLOB
  "/home/brucegoose/polis/2038-AugmentedNormalcy/prototype/test/main.cpp"
  "/home/brucegoose/polis/2038-AugmentedNormalcy/prototype/test/test_infrastructure/udp.cpp"
  )
if(NOT "${NEW_GLOB}" STREQUAL "${OLD_GLOB}")
  message("-- GLOB mismatch!")
  file(TOUCH_NOCREATE "/home/brucegoose/polis/2038-AugmentedNormalcy/prototype/cmake-build-debug/CMakeFiles/cmake.verify_globs")
endif()

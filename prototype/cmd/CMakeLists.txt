cmake_minimum_required(VERSION 3.22)

project(AugmentedNormalcy_Prototype_Commands LANGUAGES CXX)

include(../cmake/tools.cmake)

subdir_list(subdirs ${CMAKE_CURRENT_SOURCE_DIR})

foreach(subdir ${subdirs})
    file(GLOB sources CONFIGURE_DEPENDS ${subdir}/*.cpp)
    add_executable(${subdir} ${sources})
endforeach()
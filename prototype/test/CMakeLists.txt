
cmake_minimum_required(VERSION 3.22)

project(AugmentedNormalcy_Prototype_Tests LANGUAGES CXX)

include_directories(${root_dir}/third_party/doctest)

file(GLOB_RECURSE tests CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
add_executable(${PROJECT_NAME} ${tests})
target_link_libraries(${PROJECT_NAME} ${source_library})
target_include_directories(${PROJECT_NAME} PRIVATE ${source_path})

target_include_directories(${PROJECT_NAME}
        PRIVATE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/include
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>)
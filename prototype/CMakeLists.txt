cmake_minimum_required(VERSION 3.22)

project (
    AugmentedNormalcy_Prototype
    VERSION 1.0
    LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(
        FATAL_ERROR
        "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
    )
endif()

set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_CXX_FLAGS_DEBUG_INIT "-Wall")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Wall")

set(root_dir ${CMAKE_CURRENT_LIST_DIR})

# Boost asio
if (WIN32)
    # I hate windows...
    # set(Boost_DEBUG ON)
    set(BOOST_ROOT "C:\\Program Files\\boost_1_80_0")
    set(BOOST_INCLUDEDIR  "C:\\Program Files\\boost_1_80_0")
    include_directories(${BOOST_INCLUDEDIR})
    set(BOOST_LIBRARYDIR "C:\\Program Files\\boost_1_80_0\\stage\\lib")
    set(Boost_ARCHITECTURE "-x64")
else()
    set(BOOST_ROOT "/usr/local/lib/boost_1_80_0")
    set(Boost_INCLUDE_DIR  "/usr/local/lib/boost_1_80_0")
    set(Boost_LIBRARY_DIR "/usr/local/lib/boost_1_80_0/stage/lib")
endif()
set(Boost_NO_WARN_NEW_VERSIONS 1)
find_package(Boost 1.80.0 REQUIRED COMPONENTS system regex)

# Sqllite
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/third_party/SQLiteCpp)
link_libraries(
    SQLiteCpp
    sqlite3
    pthread
    dl
)

# nlohmann/json
include_directories(${CMAKE_CURRENT_LIST_DIR}/third_party/nlohmann)


file(GLOB_RECURSE public_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")
file(GLOB_RECURSE internal_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/internal/*.hpp")
file(GLOB_RECURSE internal_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/internal/*.cpp")
add_library(${PROJECT_NAME} ${internal_sources} ${internal_headers})
if (MINGW)
    target_link_libraries(${PROJECT_NAME} ws2_32)
endif()
set(source_library ${PROJECT_NAME})
set(source_path ${CMAKE_CURRENT_SOURCE_DIR}/internal)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cmd ${CMAKE_BINARY_DIR}/cmd)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test ${CMAKE_BINARY_DIR}/test)
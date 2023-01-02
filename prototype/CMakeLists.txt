cmake_minimum_required(VERSION 3.22)

project (
    AugmentedNormalcy_Prototype
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_VERBOSE_MAKEFILE ON)

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
set(third_party_dir ${CMAKE_CURRENT_LIST_DIR}/third_party)


if (WIN32)
    # I hate windows...
    # set(Boost_DEBUG ON)
    set(BOOST_ROOT "C:\\Program Files\\boost_1_80_0")
    set(BOOST_INCLUDEDIR  "C:\\Program Files\\boost_1_80_0")
    include_directories(${BOOST_INCLUDEDIR})
    set(BOOST_LIBRARYDIR "C:\\Program Files\\boost_1_80_0\\stage\\lib")
    set(Boost_ARCHITECTURE "-x64")
    set(Boost_COMPILER "-vc143")
    set(Boost_LIB_PREFIX "lib")

    set(CUDA_TOOLKIT_ROOT_DIR "C:/Program\ Files/NVIDIA\ GPU\ Computing\ Toolkit/CUDA/v12.0")
    set(CMAKE_CUDA_COMPILER "${CUDA_TOOLKIT_ROOT_DIR}/bin/nvcc.exe")

    # bad practice; just hardcoding cuda versions on bruce's laptops. use `$ nvidia-smi` to find your
    # card number, look up the cards architecture, then compare on this site for sm / compute versions
    # https://arnon.dk/tag/gencode/
    set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS};-gencode arch=compute_75,code=\"sm_75,compute_75\")
    set(CUDA_ARCHITECTURE_SETTING "75;75;75")

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_WIN32_WINNT=0x0A00 /D_WIN32")
    set(CUDA_CUDART_LIBRARY "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64/cudart.lib")
    set(CUDART_LIB "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64/cudart.lib")
    set(CUDA_CUDA_LIBRARY "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64/cuda.lib")
    set(CUDA_INCLUDE_DIR "${CUDA_TOOLKIT_ROOT_DIR}/include")
else()

    set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS};-gencode arch=compute_50,code=\"sm_50,compute_50\")
    set(CUDA_ARCHITECTURE_SETTING "50;50;50")

    set(BOOST_ROOT "/usr/local/lib/boost_1_80_0")
    set(Boost_INCLUDE_DIR  "/usr/local/lib/boost_1_80_0")
    set(Boost_LIBRARY_DIR "/usr/local/lib/boost_1_80_0/stage/lib")

endif()


# Boost asio
set(Boost_NO_WARN_NEW_VERSIONS 1)
find_package(Boost 1.80.0 REQUIRED COMPONENTS system regex)

# Sqllite
add_subdirectory(${third_party_dir}/SQLiteCpp)


# nlohmann/json
include_directories(${third_party_dir}/nlohmann)

find_package(CUDA)
if (CUDA_FOUND)
    if(WIN32)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_CUDA_CODEC_")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_CUDA_CODEC_")
    endif()
    include_directories(${CUDA_INCLUDE_DIR})
    add_subdirectory(${third_party_dir}/NvidiaCodec)
    include_directories(${third_party_dir}/NvidiaCodec)
endif()


set(source_library ${PROJECT_NAME})
set(source_path ${CMAKE_CURRENT_SOURCE_DIR}/internal)
set(udp_path ${source_path}/infrastructure/udp)
set(codec_path ${source_path}/infrastructure/codec)
set(
    internal_sources
    ${udp_path}/client.cpp
    ${codec_path}/decoder.cpp
    ${codec_path}/encoder.cpp
)
set(
    internal_headers
    ${codec_path}/bsp_packet.hpp
    ${codec_path}/codec.hpp
    ${codec_path}/decoder.hpp
    ${codec_path}/encoder.hpp
)

add_library(${source_library} STATIC ${internal_sources} ${internal_headers})
target_include_directories(${PROJECT_NAME} PUBLIC ${source_path})
target_link_libraries(${source_library} PUBLIC SQLiteCpp sqlite3)
if (!WIN32)
    target_link_libraries(${source_library} PUBLIC pthread dl)
endif()
if (CUDA_FOUND)
    target_link_libraries(${PROJECT_NAME} PUBLIC NvidiaCodec ${CUDA_CUDA_LIBRARY})
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cmd ${CMAKE_BINARY_DIR}/cmd)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test ${CMAKE_BINARY_DIR}/test)
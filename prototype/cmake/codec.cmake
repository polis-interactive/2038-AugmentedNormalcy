
if (NOT ${AN_PLATFORM} STREQUAL RPI)

    if (AN_PLATFORM STREQUAL BROOSE_WINDOWS_LAPTOP)
        set(CUDA_TOOLKIT_ROOT_DIR "C:/Program\ Files/NVIDIA\ GPU\ Computing\ Toolkit/CUDA/v12.0")
        set(CMAKE_CUDA_COMPILER "${CUDA_TOOLKIT_ROOT_DIR}/bin/nvcc.exe")

        # use `$ nvidia-smi` to find your
        # card number, look up the cards architecture, then compare on this site for sm / compute versions
        # https://arnon.dk/tag/gencode/
        set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS};-gencode arch=compute_75,code=\"sm_75,compute_75\")
        set(CUDA_ARCHITECTURE_SETTING "75;75;75")

        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_WIN32_WINNT=0x0A00 /D_WIN32")
        set(CUDA_CUDART_LIBRARY "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64/cudart.lib")
        set(CUDART_LIB "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64/cudart.lib")
        set(CUDA_CUDA_LIBRARY "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64/cuda.lib")
        set(CUDA_INCLUDE_DIR "${CUDA_TOOLKIT_ROOT_DIR}/include")

    elseif (AN_PLATFORM STREQUAL BROOSE_LINUX_LAPTOP)
        set(CUDA_TOOLKIT_ROOT_DIR /usr/local/cuda)
        set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS};-gencode arch=compute_50,code=\"sm_50,compute_50\")
        set(CUDA_ARCHITECTURE_SETTING "50;50;50")
        set(CMAKE_CUDA_COMPILER "${CUDA_TOOLKIT_ROOT_DIR}/bin/nvcc")
        set(CMAKE_CUDA_ARCHITECTURES "50")
        set(CUDA_INCLUDE_DIR "${CUDA_TOOLKIT_ROOT_DIR}/include")
    endif()

    find_package(CUDA)
    if (CUDA_FOUND)
        set(CUDA_AVAILABLE 1)
        if(WIN32)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_CUDA_CODEC_")
        else()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_CUDA_CODEC_")
        endif()
        include_directories(${CUDA_INCLUDE_DIR})
        add_subdirectory(${third_party_dir}/NvidiaCodec [binary_dir])
        include_directories(${third_party_dir}/NvidiaCodec)
    endif()

endif()

if (NOT AN_PLATFORM STREQUAL BROOSE_WINDOWS_LAPTOP)
    set(V4L2_AVAILABLE 1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_V4L2_CODEC_")
endif()
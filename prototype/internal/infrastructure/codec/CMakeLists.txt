cmake_minimum_required(VERSION 3.22)

set(SOURCES codec.cpp)

if(CUDA_FOUND)
    set(SOURCES ${SOURCES} cuda/cuda_codec.cpp cuda/cuda_decoder.cpp cuda/cuda_encoder.cpp)
    set(TARGET_LIBS NvidiaCodec ${CUDA_CUDA_LIBRARY})
endif()

add_library(codec STATIC ${SOURCES})
target_link_libraries(codec ${TARGET_LIBS})
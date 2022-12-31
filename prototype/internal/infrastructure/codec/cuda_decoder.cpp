//
// Created by bruce on 12/31/2022.
//

#include "infrastructure/codec/cuda_context.hpp"
#include "infrastructure/codec/decoder.hpp"

#include <cuda.h>
#include "Utils/NvCodecUtils.h"


namespace infrastructure {
    void Decoder::CreateDecoder() {
        std::cout << "Creating decoder" << std::endl;
        ck(cuInit(0));
        int nGpu = 0;
        ck(cuDeviceGetCount(&nGpu));
        std::cout << "Cuda GPU's found: " << nGpu << std::endl;
    }
    void Decoder::StartDecoder() {
        std::cout << "Starting decoder" << std::endl;
    }
    void Decoder::StopDecoder() {
        std::cout << "Stopping decoder" << std::endl;
    }
}

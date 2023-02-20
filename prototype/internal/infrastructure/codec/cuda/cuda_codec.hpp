//
// Created by bruce on 1/1/2023.
//

#ifndef INFRASTRUCTURE_CODEC_CUDA_CODEC_HPP
#define INFRASTRUCTURE_CODEC_CUDA_CODEC_HPP

#include <tuple>
#include <memory>
#include <cuda.h>
#include <stdexcept>
#include <string>
#include <iostream>

#include "NvInterface/cuviddec.h"
#include "NvUtils/NvCodecUtils.h"

#include "../codec.hpp"


namespace Codec {

    class CudaContext: public Context {
    public:
        CudaContext() {
            cu_check(cuInit(0), "Failed to init cuda");
            int nGpu;
            cu_check(cuDeviceGetCount(&nGpu), "Failed to get gpu count");
            if (nGpu != 1) {
                throw std::domain_error("There is more than 1 gpu? you have some programming to do!");
            }
            CUdevice cuDevice = 0;
            cu_check(cuDeviceGet(&cuDevice, 0), "Couldn't get device 0?");
            char szDeviceName[80];
            cu_check(
                cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice),
                "Couldn't get device name"
            );
            std::cout << "GPU in use: " << szDeviceName << std::endl;
            _context = std::make_shared<CUcontext>();
            cu_check(cuCtxCreate(_context.get(), 0, cuDevice), "Couldn't create context");
        }
        ~CudaContext() {
            if (_context) {
                cu_check(cuCtxDestroy(*_context), "Couldn't destroy context");
            }
        }
        std::shared_ptr<CUcontext> _context = nullptr;
    };

}


#endif //INFRASTRUCTURE_CODEC_CUDA_CODEC_HPP

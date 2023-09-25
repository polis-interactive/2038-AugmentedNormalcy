//
// Created by bruce on 12/31/2022.
//

#ifndef INFRASTRUCTURE_CODEC_CUDA_DECODER_HPP
#define INFRASTRUCTURE_CODEC_CUDA_DECODER_HPP

#include "NvDecoder/NvDecoder.h"

#include "cuda_codec.hpp"

#include <deque>
#include <functional>
#include <utility>


namespace Codec {

    class CudaDecoder: public Decoder {
    public:
        CudaDecoder(
           const Config &config, std::shared_ptr<Context> &context,
           std::function<void(std::shared_ptr<void>)> send_callback
        ) :
           Decoder(config, context, std::move(send_callback))
        {
            CreateDecoder(config, context);
        }
    private:
        void CreateDecoder(const Config &config, std::shared_ptr<Context> &context) final;
        void ThreadStartup() final;
        void TryDecode(std::shared_ptr<void> &&qp) final;
        void StopDecoder() final;

        void DecodeFrame(std::unique_ptr<BspPacket> &&frame);
        void SendDecodedFrame();
        void TryFreeMemory();
        void WaitFreeMemory();

        std::unique_ptr<NvDecoder> _decoder = {nullptr};
        std::deque<std::shared_ptr<void>> _gpu_buffers;
    };
}

#endif //INFRASTRUCTURE_CODEC_CUDA_DECODER_HPP

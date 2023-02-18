//
// Created by brucegoose on 2/3/23.
//

#include "codec.hpp"

#include <memory>

#include "cuda/cuda_codec.hpp"
#include "cuda/cuda_decoder.hpp"
#include "cuda/cuda_encoder.hpp"

#include "v4l2/v4l2_encoder.hpp"

namespace Codec {

    std::shared_ptr<Context> Context::Create(const Config &config) {
        switch(config.get_codec_type()) {
#ifdef _CUDA_CODEC_
            case Type::CUDA:
                return std::make_shared<CudaContext>();
#endif
#ifdef _V4L2_CODEC_
            case Type::V4L2:
                return std::make_shared<Context>();
#endif
            default:
                throw std::runtime_error("Selected codec unavailable... ");
        }
    }

    std::shared_ptr<Encoder> Encoder::Create(
        const Codec::Config &config, std::shared_ptr<Context> &context,
        SendCallback &&payload_sender
    ) {
        switch(config.get_codec_type()) {
#ifdef _CUDA_CODEC_
            case Type::CUDA:
                return std::make_shared<CudaEncoder>(config, context, std::move(payload_sender));
#endif
#ifdef _V4L2_CODEC_
            case Type::V4L2:
                return std::make_shared<V4l2Encoder>(config, context, std::move(payload_sender));
#endif
            default:
                throw std::runtime_error("Selected codec unavailable... ");
        }
    }

    Encoder::Encoder(
       const Codec::Config &config, std::shared_ptr<Context> &context,
       std::function<void(std::shared_ptr<void>)> &&send_callback
    ) :
        _wt(utility::WorkerThread<void>::CreateWorkerThread(
            std::bind_front(&Encoder::TryEncode, this)
        )),
        _send_callback(std::move(send_callback)),
        _b_pool(config.get_encoder_buffer_count(), [](){ return std::make_shared<SizedPayloadBuffer>(); })
    {}

    std::shared_ptr<Decoder> Decoder::Create(
        const Config &config, std::shared_ptr<Context> &context,
        SendCallback &&send_callback
    ) {
        switch(config.get_codec_type()) {
#ifdef _CUDA_CODEC_
            case Type::CUDA:
                return std::make_shared<CudaDecoder>(config, context, std::move(send_callback));
#endif
            default:
                throw std::runtime_error("Selected codec unavailable... ");
        }
    }

    Decoder::Decoder(
        const Codec::Config &config, std::shared_ptr<Context> &context,
        SendCallback &&send_callback
    ):
        _wt(utility::WorkerThread<void>::CreateWorkerThread(
            std::bind_front(&Decoder::TryDecode, this),
            std::bind_front(&Decoder::ThreadStartup, this)
        )),
        _send_callback(std::move(send_callback))
    {}


}

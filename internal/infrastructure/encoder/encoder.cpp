//
// Created by brucegoose on 5/16/23.
//

#include "encoder.hpp"
#include "sw_encoder.hpp"
#include "null_encoder.hpp"

namespace infrastructure {
    std::shared_ptr<Encoder> Encoder::Create(const EncoderConfig &config, SizedBufferCallback &&send_callback) {
        switch(config.get_encoder_type()) {
            case EncoderType::SW:
                return std::make_shared<SwEncoder>(config, std::move(send_callback));
            case EncoderType::NLL:
                return std::make_shared<NullEncoder>(config, std::move(send_callback));
            default:
                throw std::runtime_error("Selected encoder unavailable... ");
        }
    }

    Encoder::Encoder(const EncoderConfig &config, SizedBufferCallback &&send_callback):
        _send_callback(std::move(send_callback))
    {}
}
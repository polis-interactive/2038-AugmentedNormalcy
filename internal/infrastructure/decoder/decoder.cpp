//
// Created by brucegoose on 5/16/23.
//

#include "decoder.hpp"
#include "sw_decoder.hpp"
#include "null_decoder.hpp"

namespace infrastructure {
    std::shared_ptr<Decoder> Decoder::Create(const DecoderConfig &config, DecoderBufferCallback &&send_callback) {
        switch(config.get_decoder_type()) {
            case DecoderType::SW:
                return std::make_shared<SwDecoder>(config, std::move(send_callback));
            case DecoderType::NLL:
                return std::make_shared<NullDecoder>(config, std::move(send_callback));
            default:
                throw std::runtime_error("Selected decoder unavailable... ");
        }
    }

    Decoder::Decoder(const DecoderConfig &config, DecoderBufferCallback &&send_callback):
        _send_callback(std::move(send_callback))
    {}
}
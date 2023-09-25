//
// Created by brucegoose on 5/16/23.
//

#include "null_decoder.hpp"

namespace infrastructure {
    NullDecoder::NullDecoder(const infrastructure::DecoderConfig &config, DecoderBufferCallback &&send_callback):
        Decoder(config, std::move(send_callback))
    {}
    void NullDecoder::PostJpegBuffer(std::shared_ptr<SizedBuffer> &&buffer) {
        // this isn't safe, but we only need null decoder for testing so w.e
        auto fake_decoder_buffer = std::static_pointer_cast<DecoderBuffer>(buffer);
        _send_callback(std::move(fake_decoder_buffer));
    }
    void NullDecoder::StartDecoder() {}
    void NullDecoder::StopDecoder() {}
}
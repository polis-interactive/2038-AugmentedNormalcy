//
// Created by brucegoose on 5/16/23.
//

#include "null_encoder.hpp"

namespace infrastructure {
    NullEncoder::NullEncoder(const EncoderConfig &config, SizedBufferCallback &&send_callback):
        Encoder(config, std::move(send_callback))
    {}
    void NullEncoder::PostCameraBuffer(std::shared_ptr<CameraBuffer> &&buffer) {
        _send_callback(std::move(buffer));
    }
    void NullEncoder::StartEncoder() {}
    void NullEncoder::StopEncoder() {}
}
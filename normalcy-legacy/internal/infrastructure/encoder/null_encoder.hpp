//
// Created by brucegoose on 5/16/23.
//

#ifndef INFRASTRUCTURE_ENCODER_NULL_ENCODER_HPP
#define INFRASTRUCTURE_ENCODER_NULL_ENCODER_HPP

#include "encoder.hpp"

namespace infrastructure {
    class NullEncoder: public Encoder {
    public:
        NullEncoder(const EncoderConfig &config, SizedBufferCallback &&send_callback);
        void PostCameraBuffer(std::shared_ptr<CameraBuffer> &&buffer) override;
    private:
        void StartEncoder() override;
        void StopEncoder() override;
    };
}

#endif //INFRASTRUCTURE_ENCODER_NULL_ENCODER_HPP

//
// Created by brucegoose on 5/16/23.
//

#ifndef INFRASTRUCTURE_ENCODER_HPP
#define INFRASTRUCTURE_ENCODER_HPP

#include "utils/buffers.hpp"

namespace infrastructure {

    enum class EncoderType {
        SW,
        NONE,
    };

    struct EncoderConfig {
        [[nodiscard]] virtual EncoderType get_encoder_type() const = 0;
        [[nodiscard]] virtual unsigned int get_encoder_downstream_buffer_count() const = 0;
        [[nodiscard]] virtual std::pair<int, int> get_encoder_width_height() const = 0;
    };

    class Encoder {
    public:
        [[nodiscard]] static std::shared_ptr<Encoder> Create(
            const EncoderConfig &config, SizedBufferCallback &&send_callback
        );
        Encoder(const EncoderConfig &config, SizedBufferCallback &&send_callback);
        void Start() {
            StartEncoder();
        }
        virtual void PostCameraBuffer(std::shared_ptr<CameraBuffer> &&buffer) = 0;
        void Stop() {
            StopEncoder();
        }
    protected:
        SizedBufferCallback _send_callback;
    private:
        virtual void StartEncoder() = 0;
        virtual void StopEncoder() = 0;
    };

}

#endif //INFRASTRUCTURE_ENCODER_HPP

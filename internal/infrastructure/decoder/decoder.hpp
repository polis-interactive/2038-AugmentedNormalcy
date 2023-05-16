//
// Created by brucegoose on 5/16/23.
//

#ifndef INFRASTRUCTURE_DECODER_HPP
#define INFRASTRUCTURE_DECODER_HPP

#include "utils/buffers.hpp"

namespace infrastructure {

    enum class DecoderType {
        SW,
        NLL,
    };

    struct DecoderConfig {
        [[nodiscard]] virtual DecoderType get_decoder_type() const = 0;
        [[nodiscard]] virtual unsigned int get_decoder_downstream_buffer_count() const = 0;
        [[nodiscard]] virtual std::pair<int, int> get_decoder_width_height() const = 0;
    };

    class Decoder {
    public:
        [[nodiscard]] static std::shared_ptr<Decoder> Create(
            const DecoderConfig &config, DecoderBufferCallback &&send_callback
        );
        Decoder(const DecoderConfig &config, DecoderBufferCallback &&send_callback);
        void Start() {
            StartDecoder();
        }
        virtual void PostJpegBuffer(std::shared_ptr<SizedBuffer> &&buffer) = 0;
        void Stop() {
            StopDecoder();
        }
    protected:
        DecoderBufferCallback _send_callback;
    private:
        virtual void StartDecoder() = 0;
        virtual void StopDecoder() = 0;
    };

}

#endif //INFRASTRUCTURE_DECODER_HPP

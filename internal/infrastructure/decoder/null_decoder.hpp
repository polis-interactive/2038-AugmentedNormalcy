//
// Created by brucegoose on 5/16/23.
//

#ifndef INFRASTRUCTURE_DECODER_NULL_DECODER_HPP
#define INFRASTRUCTURE_DECODER_NULL_DECODER_HPP

#include "decoder.hpp"

namespace infrastructure {

    class NullDecoder: public Decoder {
    public:
        NullDecoder(const DecoderConfig &config, DecoderBufferCallback &&send_callback);
        void PostJpegBuffer(std::shared_ptr<SizedBuffer> &&buffer) override;
    private:
        void StartDecoder() override;
        void StopDecoder() override;
    };

}

#endif //INFRASTRUCTURE_DECODER_NULL_DECODER_HPP

//
// Created by bruce on 12/31/2022.
//

#include "infrastructure/codec/decoder.hpp"

#include "NvUtils/NvCodecUtils.h"


namespace infrastructure {
    void Decoder::CreateDecoder(const CodecConfig &config, CodecContext &context) {
        std::cout << "Creating decoder" << std::endl;
        const auto &[width, height] = config.get_codec_width_height();
        auto decoder = NvDecoder(        *context._context, true, cudaVideoCodec_H264, true, true, NULL, NULL,
                                         false, width, height, 1000, true);
    }
    void Decoder::StartDecoder() {
        std::cout << "Starting decoder" << std::endl;
    }
    void Decoder::ResetDecoder() {
        std::cout << "Resetting decoder" << std::endl;
    }
    void Decoder::CopyToDecoder(std::unique_ptr<BspPacket> &&frame) {
        std::cout << "Copying to decoder" << std::endl;
    }
    void Decoder::DecodeFrame() {
        std::cout << "Decoding frame" << std::endl;
    }
    void Decoder::StopDecoder() {
        std::cout << "Stopping decoder" << std::endl;
    }
    void Decoder::TryFreeMemory() {
        std::cout << "Freeing memory" << std::endl;
    }

}

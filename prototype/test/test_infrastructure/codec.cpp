//
// Created by bruce on 12/31/2022.
//

#include <doctest.h>

#include "infrastructure/codec/codec.hpp"
#include "infrastructure/codec/decoder.hpp"

struct TestConfig:
infrastructure::CodecConfig {
    [[nodiscard]] std::tuple<int, int> get_codec_width_height() const override {
        return std::tuple{1920, 1080};
    }
    [[nodiscard]] virtual cudaVideoCodec get_codec_type() const override {
        return cudaVideoCodec_H264;
    }
};



TEST_CASE("Just messing with cuda") {
    auto conf = TestConfig();
    auto ctx = infrastructure::CodecContext();
    {
        auto dec = infrastructure::Decoder(conf, ctx);
        dec.Start();
        dec.Stop();
    }
}
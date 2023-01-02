//
// Created by bruce on 12/31/2022.
//

#include <doctest.h>

#include "infrastructure/codec/codec.hpp"
#include "infrastructure/codec/decoder.hpp"
#include "infrastructure/codec/encoder.hpp"

struct TestConfig:
infrastructure::CodecConfig {
    [[nodiscard]] std::tuple<int, int> get_codec_width_height() const override {
        return std::tuple{1920, 1080};
    }
    [[nodiscard]] bool get_is_60_fps() const override {
        return true;
    }
};



TEST_CASE("Just messing with cuda") {
    auto conf = TestConfig();
    auto ctx = infrastructure::CodecContext();
    {
        auto callback = [](std::shared_ptr<uint8_t *> ptr){
            std::cout << ptr.get() << std::endl;
        };
        auto dec = infrastructure::Decoder(conf, ctx, callback);
        dec.Start();
        dec.Stop();
    }
    {
        auto enc = infrastructure::Encoder(conf, ctx);
        enc.Start();
        enc.Stop();
    }
}
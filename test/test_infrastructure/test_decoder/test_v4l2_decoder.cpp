//
// Created by brucegoose on 4/12/23.
//

#include <doctest.h>

#include <iostream>
#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;

#include "infrastructure/decoder/v4l2_decoder.hpp"

class TestV4l2DecoderConfig: public infrastructure::DecoderConfig {
    [[nodiscard]] unsigned int get_decoder_upstream_buffer_count() const override {
        return 4;
    };
    [[nodiscard]] unsigned int get_decoder_downstream_buffer_count() const override {
        return 4;
    };
    [[nodiscard]] std::pair<int, int> get_decoder_width_height() const override {
        return { 1536, 864 };
    };
};

TEST_CASE("INFRASTRUCTURE_DECODER_V4L2_DECODER-Start_And_Stop") {
    TestV4l2DecoderConfig conf;
    std::chrono::time_point< std::chrono::high_resolution_clock> t1, t2, t3, t4, t5;
    {
        t1 = Clock::now();
        auto decoder = infrastructure::V4l2Decoder::Create(conf, [](std::shared_ptr<DecoderBuffer> &&buffer) { });
        t2 = Clock::now();
        decoder->Start();
        t3 = Clock::now();
        decoder->Stop();
        t4 = Clock::now();
    }
    t5 = Clock::now();

    auto d1 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
    auto d2 = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2);
    auto d3 = std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3);
    auto d4 = std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4);
    std::cout << "test_infrastructure/decoder/v4l2_decoder startup and teardown: " <<
              d1.count() << ", " << d2.count() << ", " << d3.count() <<
              d4.count() << ", " << std::endl;
}
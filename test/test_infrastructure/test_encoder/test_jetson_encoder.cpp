//
// Created by brucegoose on 4/4/23.
//

#include <doctest.h>
#include <iostream>
#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;

#include "infrastructure/encoder/jetson_encoder.hpp"

class TestJetsonEncoderConfig : public infrastructure::EncoderConfig {
    [[nodiscard]] unsigned int get_encoder_buffer_count() const override {
        return 4;
    };
    [[nodiscard]] std::pair<int, int> get_encoder_width_height() const override {
        return { 1536, 864 };
    };
};

TEST_CASE("INFRASTRUCTURE_ENCODER_JETSON_ENCODER-Start_and_Stop") {
    TestJetsonEncoderConfig conf;
    std::chrono::time_point< std::chrono::high_resolution_clock> t1, t2, t3, t4, t5;
    {
        t1 = Clock::now();
        auto encoder = infrastructure::Encoder::Create(conf, [](std::shared_ptr<SizedBuffer> &&buffer) { });
        t2 = Clock::now();
        encoder->Start();
        t3 = Clock::now();
        encoder->Stop();
        t4 = Clock::now();
    }
    t5 = Clock::now();
    auto d1 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
    auto d2 = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2);
    auto d3 = std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3);
    auto d4 = std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4);
    std::cout << "test_infrastructure/udp/context startup and teardown: " <<
              d1.count() << ", " << d2.count() << ", " << d3.count() << ", " <<
              d4.count() << std::endl;
}
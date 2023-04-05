//
// Created by brucegoose on 4/4/23.
//

#include <doctest.h>
#include <filesystem>
#include <fstream>
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
    std::cout << "test_infrastructure/encoder/jetson_encoder startup and teardown: " <<
              d1.count() << ", " << d2.count() << ", " << d3.count() << ", " <<
              d4.count() << std::endl;
}

TEST_CASE("INFRASTRUCTURE_ENCODER_JETSON_ENCODER-Encode_a_frame") {
    TestJetsonEncoderConfig conf;

    std::filesystem::path this_dir = TEST_DIR;
    auto in_frame = this_dir;
    in_frame /= "in.yuv";

    auto out_frame = this_dir;
    out_frame /= "out_user.jpeg";

    std::chrono::time_point<std::chrono::high_resolution_clock> in_time, out_time;

    SizedBufferCallback callback = [&out_frame, &out_time](std::shared_ptr<SizedBuffer> &&ptr) mutable {
        out_time = Clock::now();
        std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
        test_file_out.write(reinterpret_cast<char*>(ptr->GetMemory()), ptr->GetSize());
        test_file_out.flush();
        test_file_out.close();
    };

    {
        auto encoder = infrastructure::Encoder::Create(conf, std::move(callback));
        encoder->Start();
        auto buffer = encoder->GetSizedBuffer();
        std::ifstream test_file_in(in_frame, std::ios::out | std::ios::binary);
        test_file_in.read((char *)buffer->GetMemory(), 1990656);
        encoder->PostSizedBuffer(std::move(buffer));
        in_time = Clock::now();
        std::this_thread::sleep_for(50ms);
        encoder->Stop();
    }

    REQUIRE(std::filesystem::exists(out_frame));

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(out_time - in_time);
    std::cout << "Time to encode: " << d1.count() << std::endl;
}
//
// Created by brucegoose on 4/4/23.
//

#include <doctest.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;

#include "infrastructure/encoder/v4l2_encoder.hpp"

#include "fake_camera.hpp"

class TestEncoderConfig : public infrastructure::EncoderConfig {
    [[nodiscard]] unsigned int get_encoder_upstream_buffer_count() const override {
        return 6;
    };
    [[nodiscard]] unsigned int get_encoder_downstream_buffer_count() const override {
        return 6;
    };
    [[nodiscard]] std::pair<int, int> get_encoder_width_height() const override {
        return { 1536, 864 };
    };
};

TEST_CASE("INFRASTRUCTURE_ENCODER_V4L2_ENCODER-Start_and_Stop") {
    TestEncoderConfig conf;
    std::chrono::time_point< std::chrono::high_resolution_clock> t1, t2, t3, t4, t5;
    {
        t1 = Clock::now();
        auto encoder = infrastructure::V4l2Encoder::Create(conf, [](std::shared_ptr<SizedBuffer> &&buffer) { });
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
              d1.count() << ", " << d2.count() << ", " << d3.count() <<
              d4.count() << ", " << std::endl;
}

TEST_CASE("INFRASTRUCTURE_ENCODER_V4L2_ENCODER-Encode_a_frame") {
    TestEncoderConfig conf;

    std::filesystem::path this_dir = TEST_DIR;
    this_dir /= "test_infrastructure";
    this_dir /= "test_encoder";

    auto in_frame = this_dir;
    in_frame /= "in.yuv";

    auto out_frame = this_dir;
    out_frame /= "out_single.jpeg";

    if(std::filesystem::remove(out_frame)) {
        std::cout << "Removed output file" << std::endl;
    } else {
        std::cout << "No output file to remove" << std::endl;
    }

    std::chrono::time_point<std::chrono::high_resolution_clock> in_time, out_time;

    SizedBufferCallback callback = [&out_frame, &out_time](std::shared_ptr<SizedBuffer> &&ptr) mutable {
        out_time = Clock::now();
        std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
        test_file_out.write((char *)(ptr->GetMemory()), ptr->GetSize());
        test_file_out.flush();
        test_file_out.close();
    };

    std::ifstream test_file_in(in_frame, std::ios::out | std::ios::binary);
    std::array<char, 1990656> in_buf = {};
    test_file_in.read(in_buf.data(), 1990656);
    FakeCamera camera(1);

    {
        auto encoder = infrastructure::V4l2Encoder::Create(conf, std::move(callback));
        encoder->Start();
        auto buffer = camera.GetBuffer();
        memcpy((char *)buffer->GetMemory(), in_buf.data(), 1990656);
        encoder->PostCameraBuffer(std::move(buffer));
        in_time = Clock::now();
        std::this_thread::sleep_for(100ms);
        encoder->Stop();
    }

    REQUIRE(std::filesystem::exists(out_frame));

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(out_time - in_time);
    std::cout << "Time to encode: " << d1.count() << std::endl;
}


TEST_CASE("INFRASTRUCTURE_ENCODER_V4L2_ENCODER-StressTest") {

    TestEncoderConfig conf;

    std::filesystem::path this_dir = TEST_DIR;
    this_dir /= "test_infrastructure";
    this_dir /= "test_encoder";

    auto in_frame = this_dir;
    in_frame /= "in.yuv";

    std::chrono::time_point<std::chrono::high_resolution_clock> in_time, out_time;

    std::atomic<int> counter = { 0 };
    std::atomic<bool> is_done = { false };

    SizedBufferCallback callback = [&out_time, &counter, &is_done](
            std::shared_ptr<SizedBuffer> &&ptr
    ) mutable {
        if (++counter >= 300 && !is_done) {
            out_time = Clock::now();
            is_done = true;
        }
    };


    std::ifstream test_file_in(in_frame, std::ios::out | std::ios::binary);
    std::array<char, 1990656> in_buf = {};
    test_file_in.read(in_buf.data(), 1990656);

    FakeCamera camera(6);

    {
        auto encoder = infrastructure::V4l2Encoder::Create(conf, std::move(callback));
        encoder->Start();
        in_time = Clock::now();

        for (int i = 0; i < 500; i++) {
            auto buffer = camera.GetBuffer();
            memcpy((char *)buffer->GetMemory(), in_buf.data(), 1990656);
            encoder->PostCameraBuffer(std::move(buffer));
            std::this_thread::sleep_for(30ms);
        }
        std::this_thread::sleep_for(100ms);
        encoder->Stop();
    }

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(out_time - in_time);
    std::cout << "Time to encode 10s of data at 30fps: " << d1.count() << std::endl;

    std::cout << "Used frames: " << counter << std::endl;
}

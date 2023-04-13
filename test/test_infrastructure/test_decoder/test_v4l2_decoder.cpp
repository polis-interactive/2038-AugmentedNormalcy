//
// Created by brucegoose on 4/12/23.
//

#include <doctest.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
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

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    auto d2 = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2);
    auto d3 = std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3);
    auto d4 = std::chrono::duration_cast<std::chrono::milliseconds>(t5 - t4);
    std::cout << "test_infrastructure/decoder/v4l2_decoder startup and teardown: " <<
              d1.count() << ", " << d2.count() << ", " << d3.count() <<
              d4.count() << ", " << std::endl;
}

TEST_CASE("INFRASTRUCTURE_DECODER_V4L2_DECODER-One_Frame") {

    std::filesystem::path this_dir = TEST_DIR;
    this_dir /= "test_infrastructure";
    this_dir /= "test_decoder";

    auto in_frame = this_dir;
    in_frame /= "in.jpeg";

    auto out_frame = this_dir;
    out_frame /= "out_single.yuv";

    if(std::filesystem::remove(out_frame)) {
        std::cout << "Removed output file" << std::endl;
    } else {
        std::cout << "No output file to remove" << std::endl;
    }

    std::ifstream test_in_file(in_frame, std::ios::in | std::ios::binary);
    test_in_file.seekg(0, std::ios::end);
    size_t input_size = test_in_file.tellg();
    test_in_file.seekg(0, std::ios::beg);
    std::array<char, 1990656> in_buf = {};
    test_in_file.read(in_buf.data(), input_size);

    std::chrono::time_point<std::chrono::high_resolution_clock> in_time, out_time;
    auto callback = [&out_frame, &out_time](std::shared_ptr<DecoderBuffer> &&buffer) mutable {
        out_time = Clock::now();
        std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
        test_file_out.write((char *)(buffer->GetMemory()), buffer->GetSize());
        test_file_out.flush();
        test_file_out.close();
    };

    {
        TestV4l2DecoderConfig conf;
        auto decoder = infrastructure::V4l2Decoder::Create(conf, std::move(callback));
        decoder->Start();
        auto buffer = decoder->GetResizableBuffer();
        memcpy((void *)buffer->GetMemory(), (void *) in_buf.data(), input_size);
        buffer->SetSize(input_size);
        in_time = Clock::now();
        decoder->PostResizableBuffer(std::move(buffer));
        std::this_thread::sleep_for(100ms);
        decoder->Stop();
    }

    REQUIRE(std::filesystem::exists(out_frame));

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(out_time - in_time);
    std::cout << "Time to encode: " << d1.count() << std::endl;

}

TEST_CASE("INFRASTRUCTURE_DECODER_V4L2_DECODER-Stress_test") {
    std::filesystem::path this_dir = TEST_DIR;
    this_dir /= "test_infrastructure";
    this_dir /= "test_decoder";

    auto in_frame = this_dir;
    in_frame /= "in.jpeg";

    auto out_frame = this_dir;
    out_frame /= "out_stress.yuv";

    if(std::filesystem::remove(out_frame)) {
        std::cout << "Removed output file" << std::endl;
    } else {
        std::cout << "No output file to remove" << std::endl;
    }

    std::ifstream test_in_file(in_frame, std::ios::in | std::ios::binary);
    test_in_file.seekg(0, std::ios::end);
    size_t input_size = test_in_file.tellg();
    test_in_file.seekg(0, std::ios::beg);
    std::array<char, 1990656> in_buf = {};
    test_in_file.read(in_buf.data(), input_size);

    std::atomic<int> counter = { 0 };
    std::atomic<bool> is_done = { false };
    std::chrono::time_point<std::chrono::high_resolution_clock> in_time, out_time;

    auto callback = [&out_frame, &out_time, &counter, &is_done](
        std::shared_ptr<DecoderBuffer> &&buffer
    ) mutable {
        if (++counter >= 300 && !is_done) {
            out_time = Clock::now();
            is_done = true;
            std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
            test_file_out.write((char *)(buffer->GetMemory()), buffer->GetSize());
            test_file_out.flush();
            test_file_out.close();
        }
    };

    {
        TestV4l2DecoderConfig conf;
        auto decoder = infrastructure::V4l2Decoder::Create(conf, std::move(callback));
        decoder->Start();
        {
            auto buffer = decoder->GetResizableBuffer();
            memcpy((void *)buffer->GetMemory(), (void *) in_buf.data(), input_size);
            buffer->SetSize(input_size);
            decoder->PostResizableBuffer(std::move(buffer));
        }

        std::this_thread::sleep_for(100ms);
        decoder->Stop();
    }

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(out_time - in_time);
    std::cout << "Time to decode 10s of data at 30fps: " << d1.count() << std::endl;

    std::cout << "Used frames: " << counter << std::endl;
}
//
// Created by bruce on 12/31/2022.
//

#include <doctest.h>

#include <utility>
#include <filesystem>
#include <fstream>
#include <chrono>
using namespace std::literals;

#include "infrastructure/codec/codec.hpp"
#include "infrastructure/codec/decoder.hpp"
#include "infrastructure/codec/encoder.hpp"

struct TestConfig : infrastructure::CodecConfig {
    [[nodiscard]] std::tuple<int, int> get_codec_width_height() const override {
        return std::tuple{1920, 1080};
    }
    [[nodiscard]] bool get_is_60_fps() const override {
        return true;
    }
};

using payload_send_function = std::function<void(std::shared_ptr<payload_buffer> &&buffer, std::size_t buffer_size)>;

struct RiggedSender: PayloadSend {
    explicit RiggedSender(payload_buffer_pool &b_pool, payload_send_function sender) :
        _b_pool(b_pool), _sender(std::move(sender))
    {}
    std::shared_ptr<payload_buffer> GetBuffer() override {
        return std::move(_b_pool.New());
    }
    void Send(std::shared_ptr<payload_buffer> &&buffer, std::size_t buffer_size) override {
        _sender(std::move(buffer), buffer_size);
    };
    payload_buffer_pool &_b_pool;
    payload_send_function _sender;
};

struct RiggedReceiver : QueuedPayloadReceive{
    RiggedReceiver(
        payload_buffer_pool &b_pool, std::shared_ptr<payload_buffer> &&buffer,
        std::size_t buffer_size
    ):
        _b_pool(b_pool),
        _buffer(buffer),
        _bytes_received(buffer_size)
    {}
    [[nodiscard]] payload_tuple GetPayload() override {
        return { _buffer, _bytes_received};
    }
    ~RiggedReceiver() {
        _b_pool.Free(std::move(_buffer));
    }
    std::shared_ptr<payload_buffer> _buffer;
    std::size_t _bytes_received;
    payload_buffer_pool &_b_pool;
};

CUresult CopyImageToCuda(uint8_t *src_ptr, CUdeviceptr dest_ptr, int nWidth, int nHeight)
{
    CUDA_MEMCPY2D m = { 0 };
    m.WidthInBytes = nWidth;
    m.Height = nHeight;
    m.srcMemoryType = CU_MEMORYTYPE_HOST;
    m.srcDevice = (CUdeviceptr)(m.srcHost = src_ptr);
    m.srcPitch = m.WidthInBytes;
    m.dstMemoryType = CU_MEMORYTYPE_DEVICE;
    m.dstDevice = (CUdeviceptr)dest_ptr;
    m.dstPitch = m.WidthInBytes;
    return cuMemcpy2D(&m);
}

CUresult CopyCudaToImage(CUdeviceptr src_ptr, uint8_t *dest_ptr, int nWidth, int nHeight)
{
    CUDA_MEMCPY2D m = { 0 };
    m.WidthInBytes = nWidth;
    m.Height = nHeight;
    m.srcMemoryType = CU_MEMORYTYPE_DEVICE;
    m.srcDevice = (CUdeviceptr)src_ptr;
    m.srcPitch = m.WidthInBytes;
    m.dstMemoryType = CU_MEMORYTYPE_HOST;
    m.dstHost = dest_ptr;
    m.dstDevice = (CUdeviceptr)(m.dstHost);
    m.dstPitch = m.WidthInBytes;
    return cuMemcpy2D(&m);
}

TEST_CASE("Let's just get an encoder running") {
    auto conf = TestConfig();
    auto ctx = infrastructure::CodecContext();
    auto b_pool = payload_buffer_pool(
        1, [](){ return std::make_shared<payload_buffer>(); }
    );

    auto rigged_sender = std::make_shared<RiggedSender>(
        b_pool,
        [&b_pool](std::shared_ptr<payload_buffer> &&buffer, std::size_t buffer_size) {
            auto rigged_receiver = std::make_shared<RiggedReceiver>(b_pool, std::move(buffer), buffer_size);
            auto [payload, size] = rigged_receiver->GetPayload();

            std::filesystem::path out_frame = TEST_DIR;
            out_frame /= "test_infrastructure";
            out_frame /= "out.h264";
            std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
            REQUIRE(test_file_out.is_open());
            test_file_out.write(reinterpret_cast<char*>(payload->data() + infrastructure::BspPacket::HeaderSize()), size - infrastructure::BspPacket::HeaderSize());
            test_file_out.flush();
            test_file_out.close();
        }
    );
    infrastructure::Encoder enc(conf, ctx, rigged_sender);
    enc.Start();

    // no we input a cuda mapped image from this file, and the pipeline should write out a modifed version of that file.
    // to check "success", the output should look something like the input file
    std::filesystem::path test_frame = TEST_DIR;
    test_frame /= "test_infrastructure";
    test_frame /= "test.nv12";
    std::ifstream test_file_in(test_frame, std::ios::in | std::ios::binary | std::ios::ate);
    REQUIRE(test_file_in.is_open());
    test_file_in.seekg( 0, std::ios::end );
    std::size_t size = test_file_in.tellg();
    REQUIRE_EQ(size, 3110400);
    auto data = new uint8_t [size];
    test_file_in.seekg (0, std::ios::beg);
    test_file_in.read (reinterpret_cast<char *>(data), size);
    test_file_in.close();

    auto data_buffer = std::shared_ptr<uint8_t[]>(data);

    // run the pipeline, wait for completion
    enc.QueueEncode(std::move(data_buffer));
    std::this_thread::sleep_for(10ms);

    enc.Stop();
}

TEST_CASE("Just messing with cuda") {
    auto conf = TestConfig();
    auto ctx = infrastructure::CodecContext();
    auto b_pool = payload_buffer_pool(
        6, [](){ return std::make_shared<payload_buffer>(); }
    );
    auto callback = [](std::shared_ptr<GpuBuffer> ptr){
        std::filesystem::path out_frame = TEST_DIR;
        out_frame /= "test_infrastructure";
        out_frame /= "out.nv12";
        std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
        std::unique_ptr<uint8_t[]> data_out(new uint8_t[3110400]);
        auto res = CopyCudaToImage((CUdeviceptr)ptr.get(), reinterpret_cast<uint8_t*>(data_out.get()), 1920, 1620);
        REQUIRE_EQ(res, CUDA_SUCCESS);
        test_file_out.write(reinterpret_cast<char*>(data_out.get()), 3110400);
        test_file_out.flush();
        test_file_out.close();
    };
    auto dec = infrastructure::Decoder(conf, ctx, callback);

    auto rigged_sender = std::make_shared<RiggedSender>(
        b_pool,
        [&b_pool, &dec](std::shared_ptr<payload_buffer> &&buffer, std::size_t buffer_size) {
            auto rigged_receiver = std::make_shared<RiggedReceiver>(b_pool, std::move(buffer), buffer_size);
            dec.QueueDecode(std::move(rigged_receiver));
        }
    );
    infrastructure::Encoder enc(conf, ctx, rigged_sender);
    enc.Start();
    dec.Start();

    // no we input a cuda mapped image from this file, and the pipeline should write out a modifed version of that file.
    // to check "success", the output should look something like the input file
    std::filesystem::path test_frame = TEST_DIR;
    test_frame /= "test_infrastructure";
    test_frame /= "test.nv12";
    std::ifstream test_file_in(test_frame, std::ios::in | std::ios::binary | std::ios::ate);
    REQUIRE(test_file_in.is_open());
    test_file_in.seekg( 0, std::ios::end );
    std::size_t size = test_file_in.tellg();
    REQUIRE_EQ(size, 3110400);
    auto data = new uint8_t [size];
    test_file_in.seekg (0, std::ios::beg);
    test_file_in.read (reinterpret_cast<char *>(data), size);
    test_file_in.close();

    CUdeviceptr gpu_ptr;
    auto res = cuMemAlloc(&gpu_ptr, 3110400);
    REQUIRE_EQ(res, CUDA_SUCCESS);
    res = CopyImageToCuda(data, gpu_ptr, 1920, 1620);
    REQUIRE_EQ(res, CUDA_SUCCESS);

    auto data_buffer = std::shared_ptr<GpuBuffer>(reinterpret_cast<uint8_t *>(gpu_ptr), [](GpuBuffer){});

    // run the pipeline, wait for completion
    enc.QueueEncode(std::move(data_buffer));
    std::this_thread::sleep_for(50ms);

    enc.Stop();
    dec.Stop();

    res = cuMemFree(gpu_ptr);
    REQUIRE_EQ(res, CUDA_SUCCESS);
    delete []data;
}
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

struct BaseTestConfig : Codec::Config {
    [[nodiscard]] std::pair<int, int> get_codec_width_height() const final {
        return {1920, 1080};
    }
    [[nodiscard]] int get_fps() const final {
        return 30;
    }
};

using payload_send_function = std::function<void(std::shared_ptr<void> &&buffer, std::size_t buffer_size)>;

struct RiggedSender: Codec::PayloadSend {
    explicit RiggedSender(payload_buffer_pool &b_pool, payload_send_function sender) :
            _b_pool(b_pool), _sender(std::move(sender))
    {}
    std::shared_ptr<void> GetBuffer() override {
        auto buf = _b_pool.New();
        auto buf_elision = std::static_pointer_cast<void>(buf);
        return std::move(buf_elision);
    }
    void Send(std::shared_ptr<void> &&buffer, std::size_t buffer_size) override {
        _sender(std::move(buffer), buffer_size);
    };
    payload_buffer_pool &_b_pool;
    payload_send_function _sender;
};

struct RiggedReceiver : Codec::QueuedPayloadReceive {
    RiggedReceiver(
       payload_buffer_pool &b_pool, std::shared_ptr<payload_buffer> &&buffer,
       std::size_t buffer_size
    ):
            _b_pool(b_pool),
            _buffer(buffer),
            _buffer_elided(std::static_pointer_cast<void>(_buffer)),
            _bytes_received(buffer_size)
    {}
    [[nodiscard]] Codec::payload_tuple GetPayload() override {
        return { std::static_pointer_cast<void>(_buffer), _bytes_received};
    }
    ~RiggedReceiver() {
        _b_pool.Free(std::move(_buffer));
    }
    std::shared_ptr<payload_buffer> _buffer;
    std::shared_ptr<void> _buffer_elided;
    std::size_t _bytes_received;
    payload_buffer_pool &_b_pool;
};

#ifdef _CUDA_CODEC_

#include <cuda.h>

struct CudaTestConfig : BaseTestConfig {
    [[nodiscard]] Codec::Type get_codec_type() const final {
        return Codec::Type::Cuda;
    }
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
    auto conf = CudaTestConfig();
    auto ctx = Codec::Context::Create(conf);
    auto b_pool = payload_buffer_pool(
            1, [](){ return std::make_shared<payload_buffer>(); }
    );

    auto rigged_sender = std::make_shared<RiggedSender>(
        b_pool,
        [&b_pool](std::shared_ptr<void> &&buffer, std::size_t buffer_size) {
            std::filesystem::path out_frame = TEST_DIR;
            out_frame /= "test_infrastructure";
            out_frame /= "test_codec";
            out_frame /= "out.h264";
            std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
            REQUIRE(test_file_out.is_open());
            test_file_out.write(
                reinterpret_cast<char*>((uint8_t *)buffer.get() + Codec::BspPacket::HeaderSize()),
                buffer_size - Codec::BspPacket::HeaderSize()
            );
            test_file_out.flush();
            test_file_out.close();
        }
    );
    auto enc = Codec::Encoder::Create(conf, ctx, rigged_sender);
    enc->Start();

    // no we input a cuda mapped image from this file, and the pipeline should write out a modifed version of that file.
    // to check "success", the output should look something like the input file
    std::filesystem::path test_frame = TEST_DIR;
    test_frame /= "test_infrastructure";
    test_frame /= "test_codec";
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

    auto data_buffer = std::shared_ptr<void>(reinterpret_cast<void *>(gpu_ptr), [](void *){});

    // run the pipeline, wait for completion
    enc->QueueEncode(std::move(data_buffer));
    std::this_thread::sleep_for(10ms);

    enc->Stop();
    res = cuMemFree(gpu_ptr);
    REQUIRE_EQ(res, CUDA_SUCCESS);
    delete []data;
}

TEST_CASE("Let's do an encode, decode cycle") {
    auto conf = CudaTestConfig();
    auto ctx = Codec::Context::Create(conf);
    auto b_pool = payload_buffer_pool(
            6, [](){ return std::make_shared<payload_buffer>(); }
    );

    std::filesystem::path out_frame = TEST_DIR;
    out_frame /= "test_infrastructure";
    out_frame /= "test_codec";
    out_frame /= "out.nv12";
    if(std::filesystem::remove(out_frame)) {
        std::cout << "Removed output file" << std::endl;
    } else {
        std::cout << "No output file to remove" << std::endl;
    }

    auto callback = [&out_frame](std::shared_ptr<void> ptr){
        std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
        std::unique_ptr<uint8_t[]> data_out(new uint8_t[3110400]);
        auto res = CopyCudaToImage((CUdeviceptr)ptr.get(), reinterpret_cast<uint8_t*>(data_out.get()), 1920, 1620);
        REQUIRE_EQ(res, CUDA_SUCCESS);
        test_file_out.write(reinterpret_cast<char*>(data_out.get()), 3110400);
        test_file_out.flush();
        test_file_out.close();
    };
    auto dec = Codec::Decoder::Create(conf, ctx, callback);

    auto rigged_sender = std::make_shared<RiggedSender>(
            b_pool,
            [&b_pool, &dec](std::shared_ptr<void> &&buffer, std::size_t buffer_size) {
                auto buffer_cast = std::static_pointer_cast<payload_buffer>(buffer);
                auto rigged_receiver = std::make_shared<RiggedReceiver>(b_pool, std::move(buffer_cast), buffer_size);
                dec->QueueDecode(std::move(rigged_receiver));
            }
    );
    auto enc = Codec::Encoder::Create(conf, ctx, rigged_sender);
    enc->Start();
    dec->Start();

    // no we input a cuda mapped image from this file, and the pipeline should write out a modifed version of that file.
    // to check "success", the output should look something like the input file
    std::filesystem::path test_frame = TEST_DIR;
    test_frame /= "test_infrastructure";
    test_frame /= "test_codec";
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

    auto data_buffer = std::shared_ptr<void>(reinterpret_cast<void *>(gpu_ptr), [](void *){});

    // run the pipeline, wait for completion
    enc->QueueEncode(std::move(data_buffer));
    std::this_thread::sleep_for(50ms);

    enc->Stop();
    dec->Stop();

    REQUIRE(std::filesystem::exists(out_frame));

    res = cuMemFree(gpu_ptr);
    REQUIRE_EQ(res, CUDA_SUCCESS);
    delete []data;
}

#endif
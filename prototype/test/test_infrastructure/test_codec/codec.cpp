//
// Created by bruce on 12/31/2022.
//

#include <doctest.h>

#include <utility>
#include <filesystem>
#include <fstream>
#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;

#include "infrastructure/codec/codec.hpp"

struct BaseTestConfig : Codec::Config {
    [[nodiscard]] std::pair<int, int> get_codec_width_height() const final {
        return {1920, 1080};
    }
    [[nodiscard]] int get_fps() const final {
        return 30;
    }
    [[nodiscard]] int get_encoder_buffer_count() const final {
        return 2;
    }
    [[nodiscard]] int get_camera_buffer_count() const final {
        return 2;
    }
};

using payload_send_function = std::function<void(std::shared_ptr<void> &&buffer, std::size_t buffer_size)>;

#ifdef _CUDA_CODEC_

#include <cuda.h>

struct CudaTestConfig : BaseTestConfig {
    [[nodiscard]] Codec::Type get_codec_type() const final {
        return Codec::Type::CUDA;
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
    auto enc = Codec::Encoder::Create(conf, ctx, [](std::shared_ptr<void> &&buffer) {
        auto in_buffer = std::static_pointer_cast<SizedPayloadBuffer>(buffer);
        std::filesystem::path out_frame = TEST_DIR;
        out_frame /= "test_infrastructure";
        out_frame /= "test_codec";
        out_frame /= "out.h264";
        std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
        REQUIRE(test_file_out.is_open());
        test_file_out.write(
                reinterpret_cast<char*>((uint8_t *)in_buffer->GetMemory() + Codec::BspPacket::HeaderSize()),
                in_buffer->GetSize() - Codec::BspPacket::HeaderSize()
        );
        test_file_out.flush();
        test_file_out.close();
    });
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

    std::filesystem::path test_dir = TEST_DIR;
    test_dir /= "test_infrastructure";
    test_dir /= "test_codec";

    std::filesystem::path out_frame = test_dir;
    out_frame /= "out.nv12";

    if(std::filesystem::remove(out_frame)) {
        std::cout << "Removed output file" << std::endl;
    } else {
        std::cout << "No output file to remove" << std::endl;
    }

    std::chrono::time_point<std::chrono::high_resolution_clock> in_time, out_time;

    auto callback = [&out_frame, &out_time](std::shared_ptr<void> ptr){
        out_time = Clock::now();
        std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
        std::unique_ptr<uint8_t[]> data_out(new uint8_t[3110400]);
        auto res = CopyCudaToImage((CUdeviceptr)ptr.get(), reinterpret_cast<uint8_t*>(data_out.get()), 1920, 1620);
        REQUIRE_EQ(res, CUDA_SUCCESS);
        test_file_out.write(reinterpret_cast<char*>(data_out.get()), 3110400);
        test_file_out.flush();
        test_file_out.close();
    };
    auto dec = Codec::Decoder::Create(conf, ctx, callback);

    auto enc = Codec::Encoder::Create(conf, ctx, [&dec](std::shared_ptr<void> &&in_buffer) {
        dec->QueueDecode(std::move(in_buffer));
    });

    enc->Start();
    dec->Start();


    // no we input a cuda mapped image from this file, and the pipeline should write out a modifed version of that file.
    // to check "success", the output should look something like the input file
    std::filesystem::path test_frame = test_dir;
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
    in_time = Clock::now();
    enc->QueueEncode(std::move(data_buffer));
    std::this_thread::sleep_for(50ms);

    enc->Stop();
    dec->Stop();

    REQUIRE(std::filesystem::exists(out_frame));

    res = cuMemFree(gpu_ptr);
    REQUIRE_EQ(res, CUDA_SUCCESS);
    delete []data;

    auto d1 = std::chrono::duration_cast<std::chrono::microseconds>(out_time - in_time);
    std::cout << "Time to transcode (backwards): " << d1.count() << std::endl;
}

#endif

#ifdef _V4L2_CODEC_

#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <unistd.h>

struct V4l2TestConfig : BaseTestConfig {
    [[nodiscard]] Codec::Type get_codec_type() const final {
        return Codec::Type::V4L2;
    }
};

class V4l2Device {
public:
    V4l2Device(const char dev[]) : _fd(open(dev, O_RDWR, 0)){
        if (_fd < 0) {
            throw std::runtime_error("failed to open V4L2 camera");
        }
        createBuffer();
    }
    ~V4l2Device() {
        destroyBuffer();
    }
    std::shared_ptr<CameraBuffer> GetBuffer() {
        return _buffer;
    }
private:
    int xioctl(unsigned long ctl, void *arg) {
        int ret, num_tries = 10;
        do
        {
            ret = ioctl(_fd, ctl, arg);
        } while (ret == -1 && errno == EINTR && num_tries-- > 0);
        return ret;
    }
    void createBuffer() {

        const int width = 1536;
        const int height = 864;
        const int stride = 1536;

        v4l2_capability cap = {};
        if(xioctl(VIDIOC_QUERYCAP, &cap) < 0){
            std::cout << "couldn't query caps" << std::endl;
            throw std::runtime_error("couldn't SET FORMAT");
        }

        if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
            std::cout << "The device does not handle single-planar video capture" << std::endl;
            throw std::runtime_error("bad caps");
        }

        v4l2_format format = {};
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
        format.fmt.pix.width = 1536;
        format.fmt.pix.height = 864;
        format.fmt.pix.bytesperline = 1536;
        format.fmt.pix.field = V4L2_FIELD_NONE;
        int ret = xioctl(VIDIOC_S_FMT, &format);
        if (ret < 0) {
            std::cout << "couldn't SET FORMAT: " << ret << std::endl;
            throw std::runtime_error("couldn't SET FORMAT");
        }

        v4l2_requestbuffers rb = {};
        rb.count = 1;
        rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        rb.memory = V4L2_MEMORY_DMABUF;
        ret = xioctl(VIDIOC_REQBUFS, &rb);
        if (ret < 0) {
            std::cout << "couldn't request buffer" << ret << std::endl;
            throw std::runtime_error("couldn't request buffer");
        }
        std::cout << "Got buffers: " << rb.count << std::endl;
        if (rb.count < 1) {
            std::cout << "couldn't request 1 buffer? " << ret << std::endl;
            throw std::runtime_error("couldn't request buffer for 1");

        }

        v4l2_buffer buffer = {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = 1;

        ret = xioctl(VIDIOC_QUERYBUF, &buffer);
        if (ret < 0) {
            std::cout << "couldn't create buffer" << ret << std::endl;
            throw std::runtime_error("couldn't create buffer");
        }

        struct v4l2_exportbuffer expbuf = {};
        expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        expbuf.index = 1;
        expbuf.flags = O_RDWR;

        ret = xioctl(VIDIOC_QUERYBUF, &expbuf);
        if (ret < 0) {
            std::cout << "couldn't export buffer" << ret << std::endl;
            throw std::runtime_error("couldn't export buffer");
        }
        std::cout << "Making for buffer length: " << buffer.length << std::endl;
        std::cout << "Making for buffer offset: " << buffer.length << std::endl;
        auto mem = mmap(
            nullptr, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, _fd,
            buffer.m.offset
        );
        std::cout << "Main fd:" << _fd << std::endl;
        std::cout << "Export fd:" << expbuf.fd << std::endl;
        auto size = buffer.length;
        _buffer = std::shared_ptr<CameraBuffer>(new CameraBuffer(mem, expbuf.fd, size), [](CameraBuffer *c) {});

        std::cout << "Created buffer" << std::endl;
    }
    void destroyBuffer() {
        if (munmap(_buffer->GetMemory(), _buffer->GetSize()) < 0) {
            std::cout << "Couldn't free mmap buffers" << std::endl;
            throw std::runtime_error("Couldn't free mmap buffers");
        }

        v4l2_requestbuffers rb = {};
        int ret;
        rb.count = 0;
        rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        rb.memory = V4L2_MEMORY_MMAP;
        ret = xioctl(VIDIOC_REQBUFS, &rb);
        if (ret < 0) {
            std::cout << "couldn't delete buffer" << ret << std::endl;
            throw std::runtime_error("couldn't delete buffer");
        }

        close(_fd);
        std::cout << "V4l2 Camera Closed" << std::endl;

    }
    int _fd ;
    std::shared_ptr<CameraBuffer> _buffer = nullptr;
};

TEST_CASE("Let's just get an encoder running") {
    auto conf = V4l2TestConfig();
    // dummy ctx, w.e
    auto ctx = Codec::Context::Create(conf);

    std::filesystem::path test_path = TEST_DIR;
    test_path /= "test_infrastructure";
    test_path /= "test_codec";

    std::filesystem::path out_frame = test_path;
    out_frame /= "out.h264";

    if(std::filesystem::remove(out_frame)) {
        std::cout << "Removed output file" << std::endl;
    } else {
        std::cout << "No output file to remove" << std::endl;
    }

    std::chrono::time_point<std::chrono::high_resolution_clock> in_time, out_time;
    auto callback = [&out_frame, &out_time](std::shared_ptr<void> out_buffer){
        auto payload_buffer = std::static_pointer_cast<SizedPayloadBuffer>(out_buffer);
        out_time = Clock::now();
        std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
        test_file_out.write(reinterpret_cast<char*>(payload_buffer->GetMemory()), payload_buffer->GetSize());
        test_file_out.flush();
        test_file_out.close();
    };

    auto enc = Codec::Encoder::Create(conf, ctx, callback);
    enc->Start();

    std::filesystem::path test_frame = test_path;
    test_frame /= "test.yuv";
    std::ifstream test_file_in(test_frame, std::ios::in | std::ios::binary | std::ios::ate);
    REQUIRE(test_file_in.is_open());
    test_file_in.seekg( 0, std::ios::end );
    std::size_t size = test_file_in.tellg();
    // might not be actual size
    REQUIRE_EQ(size, 1990656);
    auto data = new uint8_t [size];
    test_file_in.seekg (0, std::ios::beg);
    test_file_in.read (reinterpret_cast<char *>(data), size);
    test_file_in.close();

    {
        auto dev = V4l2Device("/dev/video0");
        auto buffer = dev.GetBuffer();
        memcpy(buffer->GetMemory(), data, 1990656);
        in_time = Clock::now();
        buffer->_size = 1990656;
        enc->QueueEncode(std::move(buffer));
        std::this_thread::sleep_for(50ms);
        REQUIRE(std::filesystem::exists(out_frame));
    }
    std::cout << "do I get here?" << std::endl;
    enc->Stop();
    std::cout << "do I get here x2?" << std::endl;

    delete []data;
    auto d1 = std::chrono::duration_cast<std::chrono::microseconds>(out_time - in_time);
    std::cout << "Time to encode: " << d1.count() << std::endl;

}

#endif
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
        return 1;
    };
    [[nodiscard]] unsigned int get_encoder_downstream_buffer_count() const override {
        return 1;
    };
    [[nodiscard]] std::pair<int, int> get_encoder_width_height() const override {
        return { 1536, 864 };
    };
};

#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include <linux/videodev2.h>

#include <filesystem>
#include <cstring>
#include <fstream>
#include <thread>


int jioctl(int fd, unsigned long ctl, void *arg) {
    int ret, num_tries = 10;
    do
    {
        ret = ioctl(fd, ctl, arg);
    } while (ret == -1 && errno == EINTR && num_tries-- > 0);
    return ret;
}


TEST_CASE("INFRASTRUCTURE_ENCODER_V4L2_ENCODER-Hardcoded") {
const char device_name[] = "/dev/video11";
const int width = 1536;
const int height = 864;
const int stride = 1536;
const int max_size = width * height * 3 / 2;
auto encoder_fd = open(device_name, O_RDWR, 0);
if (encoder_fd == -1) {
switch(errno) {
case ENOENT:
std::cerr << "File does not exist" << std::endl;
break;
case EACCES:
std::cerr << "permission denied" << std::endl;
break;
default:
std::cerr << "Unknown error" << std::endl;
};
return;
}
v4l2_format fmt = {0};
fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
fmt.fmt.pix_mp.width = width;
fmt.fmt.pix_mp.height = height;
fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420;
fmt.fmt.pix_mp.plane_fmt[0].bytesperline = stride;
fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;
fmt.fmt.pix_mp.num_planes = 1;

if (jioctl(encoder_fd, VIDIOC_S_FMT, &fmt))
throw std::runtime_error("failed to set output caps");


fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
fmt.fmt.pix_mp.width = width;
fmt.fmt.pix_mp.height = height;
fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MJPEG;
fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;
fmt.fmt.pix_mp.num_planes = 1;
fmt.fmt.pix_mp.plane_fmt[0].bytesperline = 0;
fmt.fmt.pix_mp.plane_fmt[0].sizeimage = 512 << 10;

if (jioctl(encoder_fd, VIDIOC_S_FMT, &fmt))
throw std::runtime_error("failed to set output caps");

v4l2_requestbuffers reqbufs = {};
reqbufs.count = 1;
reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
reqbufs.memory = V4L2_MEMORY_MMAP;
// reqbufs.memory = V4L2_MEMORY_DMABUF;
if (jioctl(encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
std::cout << errno << std::endl;
throw std::runtime_error("request for output buffers failed");
}
std::cout << "V4L2 Encoder got " << reqbufs.count << " output buffers" << std::endl;

reqbufs = {};
reqbufs.count = 1;
reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
reqbufs.memory = V4L2_MEMORY_MMAP;
if (jioctl(encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
std::cout << errno << std::endl;
throw std::runtime_error("request for capture buffers failed");
}

v4l2_plane planes[VIDEO_MAX_PLANES];
v4l2_buffer buffer = {};


buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
buffer.memory = V4L2_MEMORY_MMAP;
buffer.index = 0;
buffer.length = 1;
buffer.m.planes = planes;
if (jioctl(encoder_fd, VIDIOC_QUERYBUF, &buffer) < 0)
throw std::runtime_error("failed to query output buffer");

auto output_size = buffer.m.planes[0].length;
auto output_offset = buffer.m.planes[0].m.mem_offset;

buffer = {};
memset(planes, 0, sizeof(planes));
buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
buffer.memory = V4L2_MEMORY_MMAP;
buffer.index = 0;
buffer.length = 1;
buffer.m.planes = planes;
if (xioctl(encoder_fd, VIDIOC_QUERYBUF, &buffer) < 0)
throw std::runtime_error("failed to query capture buffer");

auto capture_size = buffer.m.planes[0].length;
auto capture_offset = buffer.m.planes[0].m.mem_offset;

buffer = {};
memset(planes, 0, sizeof(planes));
buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
buffer.memory = V4L2_MEMORY_MMAP;
buffer.index = 0;
buffer.length = 1;
buffer.m.planes = planes;
buffer.m.planes[0].length = capture_size;
buffer.m.planes[0].m.mem_offset = capture_offset;
if (jioctl(encoder_fd, VIDIOC_QBUF, &buffer) < 0)
throw std::runtime_error("failed to query capture buffer");


int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
if (jioctl(encoder_fd, VIDIOC_STREAMON, &type) < 0)
throw std::runtime_error("failed to start output");

type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
if (jioctl(encoder_fd, VIDIOC_STREAMON, &type) < 0)
throw std::runtime_error("failed to start capture");

buffer = {};
memset(planes, 0, sizeof(planes));
buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
buffer.index = 0;
buffer.field = V4L2_FIELD_NONE;
buffer.memory = V4L2_MEMORY_MMAP;
buffer.length = 1;
buffer.timestamp.tv_sec = 0;
buffer.timestamp.tv_usec = 0;
buffer.flags = V4L2_BUF_FLAG_PREPARED;
buffer.m.planes = planes;
buffer.m.planes[0].length = max_size;
buffer.m.planes[0].bytesused = max_size;
buffer.m.planes[0].m.mem_offset = output_offset;
if (jioctl(encoder_fd, VIDIOC_QBUF, &buffer) < 0)
throw std::runtime_error("failed to queue output buffer");

buffer = {};
memset(planes, 0, sizeof(planes));
buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
buffer.memory = V4L2_MEMORY_MMAP;
buffer.index = 0;
buffer.length = 1;
buffer.m.planes = planes;
buffer.m.planes[0].length = capture_size;
buffer.m.planes[0].m.mem_offset = capture_offset;

if (jioctl(encoder_fd, VIDIOC_DQBUF, &buffer) < 0)
throw std::runtime_error("failed to dequeue capture buffer");

type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
if (jioctl(encoder_fd, VIDIOC_STREAMOFF, &type) < 0)
throw std::runtime_error("failed to start output");

type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
if (jioctl(encoder_fd, VIDIOC_STREAMOFF, &type) < 0)
throw std::runtime_error("failed to start capture");

reqbufs = {};
reqbufs.count = 0;
reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
reqbufs.memory = V4L2_MEMORY_MMAP;
if (jioctl(encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
std::cout << "Failed to free output buffers" << std::endl;
}

reqbufs = {};
reqbufs.count = 0;
reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
reqbufs.memory = V4L2_MEMORY_MMAP;
if (jioctl(encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
std::cout << "Failed to free capture buffers" << std::endl;
}

close(encoder_fd);
}

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
    FakeCamera camera(5);

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

    FakeCamera camera(5);

    {
        auto encoder = infrastructure::V4l2Encoder::Create(conf, std::move(callback));
        encoder->Start();
        in_time = Clock::now();

        for (int i = 0; i < 500; i++) {
            auto buffer = camera.GetBuffer();
            if (buffer != nullptr) {
                memcpy((char *)buffer->GetMemory(), in_buf.data(), 1990656);
                encoder->PostCameraBuffer(std::move(buffer));
            }
            std::this_thread::sleep_for(30ms);
        }
        std::this_thread::sleep_for(100ms);
        encoder->Stop();
    }

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(out_time - in_time);
    std::cout << "Time to encode 10s of data at 30fps: " << d1.count() << std::endl;

    std::cout << "Used frames: " << counter << std::endl;
}

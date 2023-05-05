//
// Created by brucegoose on 5/5/23.
//

#include "fake_camera.hpp"

#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <filesystem>
#include <cstring>

int fxioctl(int fd, unsigned long ctl, void *arg) {
    int ret, num_tries = 10;
    do
    {
        ret = ioctl(fd, ctl, arg);
    } while (ret == -1 && errno == EINTR && num_tries-- > 0);
    return ret;
}

FakeCamera::FakeCamera(const int buffer_count) {
    _camera_fd = open("/dev/video0", O_RDWR, 0);

    if (_camera_fd < 0) {
        throw std::runtime_error("failed to open /dev/video0");
    }

    // Query camera capabilities
    v4l2_capability cap{};
    if (fxioctl(_camera_fd, VIDIOC_QUERYCAP, &cap) == -1) {
        perror("Querying capabilities");
        throw std::runtime_error("Querying capabilities");
    }

    // Check if the device supports video capture
    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        throw std::runtime_error("Device does not support video capture");
    }

    // Check if the device supports streaming
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        throw std::runtime_error("Device does not support streaming");
    }

    v4l2_fmtdesc fmtdesc{0};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    for (int i = 0;; ++i) {
        fmtdesc.index = i;
        if (fxioctl(_camera_fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1) {
            std::cout << "WHAT" << std::endl;
            break;
        }
        std::cout << "CAPTURE Format " << i << ": " << fmtdesc.description
                  << ", FourCC: " << static_cast<char>((fmtdesc.pixelformat >> 0) & 0xFF)
                  << static_cast<char>((fmtdesc.pixelformat >> 8) & 0xFF)
                  << static_cast<char>((fmtdesc.pixelformat >> 16) & 0xFF)
                  << static_cast<char>((fmtdesc.pixelformat >> 24) & 0xFF)
                  << std::endl;
    }


    std::pair<unsigned int, unsigned int> _width_height = { 1536, 864 };

    v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix_mp.width = _width_height.first;
    fmt.fmt.pix_mp.height = _width_height.second;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;
    fmt.fmt.pix.bytesperline = _width_height.first;
    fmt.fmt.pix.sizeimage = _width_height.first * _width_height.second * 3 / 2;

    if (ioctl(_camera_fd, VIDIOC_S_FMT, &fmt) == -1) {
        perror("ioctl VIDIOC_S_FMT failed");
        throw std::runtime_error("failed to set capture caps");
    }

    v4l2_requestbuffers reqbufs = {};
    reqbufs.count = buffer_count;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    if (fxioctl(_camera_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
        throw std::runtime_error("request for output buffers failed");
    } else if (reqbufs.count != buffer_count) {
        std::stringstream out_str;
        out_str << "Unable to return " << buffer_count << " capture buffers; only got " << reqbufs.count;
        throw std::runtime_error(out_str.str());
    }

    v4l2_plane planes[VIDEO_MAX_PLANES];
    v4l2_buffer buffer = {};
    v4l2_exportbuffer expbuf = {};

    for (int i = 0; i < buffer_count; i++) {

        buffer = {};
        memset(planes, 0, sizeof(planes));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if (fxioctl(_camera_fd, VIDIOC_QUERYBUF, &buffer) < 0)
            throw std::runtime_error("failed to query output buffer");

        /*
         * mmap
         */

        auto capture_size = buffer.length;
        auto capture_offset = buffer.m.offset;
        auto capture_mem = mmap(
                nullptr, capture_size, PROT_READ | PROT_WRITE, MAP_SHARED, _camera_fd, capture_offset
        );
        if (capture_mem == MAP_FAILED)
            throw std::runtime_error("failed to mmap output buffer");

        /*
         * export to dmabuf
         */

        memset(&expbuf, 0, sizeof(expbuf));
        expbuf.type = buffer.type;
        expbuf.index = buffer.index;
        expbuf.flags = O_RDWR;

        if (fxioctl(_camera_fd, VIDIOC_EXPBUF, &expbuf) < 0)
            throw std::runtime_error("failed to export the capture buffer");

        auto camera_buffer = new CameraBuffer(&buffer.index, capture_mem, expbuf.fd, capture_size, 0);
        _camera_buffers.push_back(camera_buffer);

    }

}

std::shared_ptr<CameraBuffer> FakeCamera::GetBuffer() {
    CameraBuffer *buffer = nullptr;
    {
        std::unique_lock<std::mutex> lock(_camera_mutex);
        if (!_camera_buffers.empty()) {
            buffer = _camera_buffers.front();
            _camera_buffers.pop_front();
        }
    }
    if (buffer == nullptr) {
        return nullptr;
    }
    auto out_buffer = std::shared_ptr<CameraBuffer>(buffer, [this](CameraBuffer *b) {
        std::unique_lock<std::mutex> lock(_camera_mutex);
        _camera_buffers.push_back(b);
    });
    return out_buffer;
}

FakeCamera::~FakeCamera() {
    for (auto &c_buffer: _camera_buffers) {
        munmap(c_buffer->GetMemory(), c_buffer->GetSize());
        close(c_buffer->GetFd());
    }
    v4l2_requestbuffers reqbufs = {};
    reqbufs.count = 0;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    if (fxioctl(_camera_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
        std::cout << "Failed to free capture buffers" << std::endl;
    }
    close(_camera_fd);
}
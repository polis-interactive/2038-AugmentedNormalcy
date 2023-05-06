//
// Created by brucegoose on 5/4/23.
//

#include "v4l2_encoder.hpp"

#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <filesystem>

#include <cstring>

int xioctl(int fd, unsigned long ctl, void *arg) {
    int ret, num_tries = 10;
    do
    {
        ret = ioctl(fd, ctl, arg);
    } while (ret == -1 && errno == EINTR && num_tries-- > 0);
    return ret;
}

namespace infrastructure {

    const char V4l2Encoder::_device_name[] = "/dev/video11";

    V4l2Encoder::V4l2Encoder(const EncoderConfig &config, SizedBufferCallback output_callback):
            _output_callback(std::move(output_callback)),
            _width_height(config.get_encoder_width_height())
    {
        auto upstream_count = config.get_encoder_upstream_buffer_count();
        auto downstream_count = config.get_encoder_downstream_buffer_count();
        setupEncoder();
        setupUpstreamBuffers(upstream_count);
        setupDownstreamBuffers(downstream_count);
    }

    void V4l2Encoder::Start() {

        if (!_work_stop) {
            return;
        }

        int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        if (xioctl(_encoder_fd, VIDIOC_STREAMON, &type) < 0)
            throw std::runtime_error("failed to start output");

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (xioctl(_encoder_fd, VIDIOC_STREAMON, &type) < 0)
            throw std::runtime_error("failed to start capture");

        _is_primed = false;
        _work_stop = false;

        auto self(shared_from_this());
        _work_thread = std::make_unique<std::thread>([this, s = std::move(self)]() mutable { run(); });

    }

    void V4l2Encoder::Stop() {
        if (_work_stop) {
            return;
        }

        if (_work_thread) {
            if (_work_thread->joinable()) {
                {
                    std::unique_lock<std::mutex> lock(_work_mutex);
                    _work_stop = true;
                    _work_cv.notify_one();
                }
                _work_thread->join();
            }
            _work_thread.reset();
        }
        // just in case we skipped above
        _work_stop = true;

        int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        if (xioctl(_encoder_fd, VIDIOC_STREAMOFF, &type) < 0)
            throw std::runtime_error("failed to stop output");

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (xioctl(_encoder_fd, VIDIOC_STREAMOFF, &type) < 0)
            throw std::runtime_error("failed to stop capture");

        /*
         * I think I should dequeue things but meh
         */

    }

    void V4l2Encoder::PostCameraBuffer(std::shared_ptr<CameraBuffer> &&buffer) {
        if (buffer == nullptr || _work_stop) {
            return;
        }
        std::unique_lock<std::mutex> lock(_work_mutex);
        _work_queue.push(std::move(buffer));
        _work_cv.notify_one();
    }

    void V4l2Encoder::run() {

        while(!_work_stop) {
            std::shared_ptr<CameraBuffer> buffer;
            {
                std::unique_lock<std::mutex> lock(_work_mutex);
                _work_cv.wait(lock, [this]() {
                    return !_work_queue.empty() || _work_stop;
                });
                if (_work_stop) {
                    return;
                } else if (_work_queue.empty()) {
                    continue;
                }
                buffer = std::move(_work_queue.front());
                _work_queue.pop();
            }
            encodeBuffer(std::move(buffer));
        }
    }

    void V4l2Encoder::encodeBuffer(std::shared_ptr<CameraBuffer> &&cam_buffer) {

        // in the future, we may need this; but as is, we run synchronously so meh
        EncoderBuffer *output_buffer = _available_upstream_buffers.front();
        if (output_buffer == nullptr) {
            throw std::runtime_error("failed to get output buffer");
        }
        memcpy(output_buffer->GetMemory(), cam_buffer->GetMemory(), cam_buffer->GetSize());


        /*
         * queue output buffer, wait for it to finish
         */

        v4l2_plane planes[VIDEO_MAX_PLANES];
        v4l2_buffer buffer = {};
        buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buffer.index = 0;
        buffer.field = V4L2_FIELD_NONE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.length = 1;
        buffer.timestamp.tv_sec = 0;
        buffer.timestamp.tv_usec = 0;
        buffer.flags = V4L2_BUF_FLAG_PREPARED;
        buffer.m.planes = planes;
        buffer.m.planes[0].length = 1990656;
        buffer.m.planes[0].bytesused = 1990656;
        buffer.m.planes[0].m.mem_offset = 0;
        if (xioctl(_encoder_fd, VIDIOC_QBUF, &buffer) < 0) {
            perror("ioctl VIDIOC_QBUF failed");
            throw std::runtime_error("failed to queue output buffer");
        }

        // may need to pump the encoder here

        if (!waitForEncoder()) {
            std::cout << "I think this should be an error..." << std::endl;
            return;
        }

        /*
         * dequeue downstream buffer; wrap it and post it
         */

        buffer = {};
        memset(planes, 0, sizeof(planes));
        buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.length = 1;
        buffer.m.planes = planes;

        int ret = xioctl(_encoder_fd, VIDIOC_DQBUF, &buffer);
        if (ret == 0) {
            auto downstream_buffer = _downstream_buffers.at(buffer.index);
            downstream_buffer->SetSize(buffer.m.planes[0].bytesused);
            auto self(shared_from_this());
            auto wrapped_buffer = std::shared_ptr<EncoderBuffer>(
                    downstream_buffer,
                    [this, s = std::move(self)](EncoderBuffer *e) {
                        queueDownstreamBuffer(e);
                    }
            );
            _output_callback(std::move(wrapped_buffer));
        }

        /*
         * dequeue upstream buffer, return it to the pool
         */

        buffer = {};
        memset(planes, 0, sizeof(planes));
        buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.length = 1;
        buffer.m.planes = planes;
        ret = xioctl(_encoder_fd, VIDIOC_DQBUF, &buffer);

    }

    bool V4l2Encoder::waitForEncoder() {
        int attempts = 3;
        while (attempts > 0) {
            pollfd p = { _encoder_fd, POLLIN, 0 };
            int ret = poll(&p, 1, 100);
            if (ret == -1) {
                if (errno == EINTR)
                    continue;
                throw std::runtime_error("unexpected errno " + std::to_string(errno) + " from poll");
            }
            if (p.revents & POLLIN)
            {
                return true;
            }
            attempts--;
        }
        return false;
    }

    void V4l2Encoder::queueDownstreamBuffer(EncoderBuffer *e) const {
        v4l2_buffer buf = {};
        v4l2_plane planes[VIDEO_MAX_PLANES] = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = e->GetIndex();
        buf.length = 1;
        buf.m.planes = planes;
        if (xioctl(_encoder_fd, VIDIOC_QBUF, &buf) < 0)
            throw std::runtime_error("failed to re-queue encoded buffer");
    }

    void V4l2Encoder::setupEncoder() {

        _encoder_fd = open(_device_name, O_RDWR, 0);

        v4l2_format fmt = {0};
        fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        fmt.fmt.pix_mp.width = _width_height.first;
        fmt.fmt.pix_mp.height = _width_height.second;
        fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420;
        fmt.fmt.pix_mp.plane_fmt[0].bytesperline = _width_height.first;
        fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
        fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;
        fmt.fmt.pix_mp.num_planes = 1;

        if (xioctl(_encoder_fd, VIDIOC_S_FMT, &fmt))
            throw std::runtime_error("failed to set output caps");


        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        fmt.fmt.pix_mp.width = _width_height.first;
        fmt.fmt.pix_mp.height = _width_height.second;
        fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MJPEG;
        fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
        fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;
        fmt.fmt.pix_mp.num_planes = 1;
        fmt.fmt.pix_mp.plane_fmt[0].bytesperline = 0;
        fmt.fmt.pix_mp.plane_fmt[0].sizeimage = 512 << 10;

        if (xioctl(_encoder_fd, VIDIOC_S_FMT, &fmt))
            throw std::runtime_error("failed to set capture caps");

    }

    void V4l2Encoder::setupUpstreamBuffers(const unsigned int request_upstream_buffers) {

        v4l2_requestbuffers reqbufs = {};
        reqbufs.count = request_upstream_buffers;
        reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        reqbufs.memory = V4L2_MEMORY_MMAP;
        if (xioctl(_encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
            throw std::runtime_error("request for output buffers failed");
        } else if (reqbufs.count != request_upstream_buffers) {
            std::stringstream out_str;
            out_str << "Unable to return " << request_upstream_buffers << " output buffers; only got " << reqbufs.count;
            throw std::runtime_error(out_str.str());
        }

        v4l2_plane planes[VIDEO_MAX_PLANES];
        v4l2_buffer buffer = {};

        for (int i = 0; i < request_upstream_buffers; i++) {

            /*
             * Get buffer
             */

            buffer = {};
            memset(planes, 0, sizeof(planes));
            buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = i;
            buffer.length = 1;
            buffer.m.planes = planes;

            if (xioctl(_encoder_fd, VIDIOC_QUERYBUF, &buffer) < 0)
                throw std::runtime_error("failed to query output buffer");

            /*
             * mmap buffer
             */

            auto output_size = buffer.m.planes[0].length;
            auto output_offset = buffer.m.planes[0].m.mem_offset;
            auto output_mem = mmap(
                    nullptr, output_size, PROT_READ | PROT_WRITE, MAP_SHARED, _encoder_fd, output_offset
            );
            if (output_mem == MAP_FAILED)
                throw std::runtime_error("failed to mmap output buffer");

            std::cout << "V4l2 Encoder MMAPed output buffer with size like so: " <<
                      output_size << ", " << output_offset << ", " << buffer.index <<
                      ", " << (void *) output_mem << std::endl;


            if (xioctl(_encoder_fd, VIDIOC_QBUF, &buffer) < 0)
                throw std::runtime_error("failed to dequeue capture buffer early");

            if (xioctl(_encoder_fd, VIDIOC_DQBUF, &buffer) < 0)
                throw std::runtime_error("failed to dequeue capture buffer early");

            /*
             * Create proxy
             */

            auto upstream_buffer = new EncoderBuffer(buffer.index, output_mem, output_size, output_offset);
            _available_upstream_buffers.push(upstream_buffer);
        }
    }

    void V4l2Encoder::setupDownstreamBuffers(unsigned int request_downstream_buffers) {

        v4l2_requestbuffers reqbufs = {};
        reqbufs.count = request_downstream_buffers;
        reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        reqbufs.memory = V4L2_MEMORY_MMAP;
        if (xioctl(_encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
            throw std::runtime_error("request for output buffers failed");
        } else if (reqbufs.count != request_downstream_buffers) {
            std::stringstream out_str;
            out_str << "Unable to return " << request_downstream_buffers << " capture buffers; only got " << reqbufs.count;
            throw std::runtime_error(out_str.str());
        }

        v4l2_plane planes[VIDEO_MAX_PLANES];
        v4l2_buffer buffer = {};

        for (int i = 0; i < request_downstream_buffers; i++) {

            /*
             * Get buffer
             */

            buffer = {};
            memset(planes, 0, sizeof(planes));
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = i;
            buffer.length = 1;
            buffer.m.planes = planes;

            if (xioctl(_encoder_fd, VIDIOC_QUERYBUF, &buffer) < 0)
                throw std::runtime_error("failed to query output buffer");

            /*
             * mmap
             */

            auto capture_size = buffer.m.planes[0].length;
            auto capture_offset = buffer.m.planes[0].m.mem_offset;
            auto capture_mem = mmap(
                    nullptr, capture_size, PROT_READ | PROT_WRITE, MAP_SHARED, _encoder_fd, capture_offset
            );
            if (capture_mem == MAP_FAILED)
                throw std::runtime_error("failed to mmap output buffer");

            /*
             * queue buffer
             */

            if (xioctl(_encoder_fd, VIDIOC_QBUF, &buffer) < 0)
                throw std::runtime_error("failed to dequeue capture buffer");

            auto downstream_buffer = new EncoderBuffer(buffer.index, capture_mem, capture_size, capture_offset);
            _downstream_buffers.insert({ buffer.index, downstream_buffer });
        }

    }

    V4l2Encoder::~V4l2Encoder() {
        Stop();
        teardownUpstreamBuffers();
        teardownDownstreamBuffers();
        close(_encoder_fd);
    }

    void V4l2Encoder::teardownUpstreamBuffers() {
        while(!_available_upstream_buffers.empty()) {
            auto buffer = _available_upstream_buffers.front();
            _available_upstream_buffers.pop();
            munmap(buffer->GetMemory(), buffer->GetSize());
            delete buffer;
        }
        v4l2_requestbuffers reqbufs = {};
        reqbufs.count = 0;
        reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        reqbufs.memory = V4L2_MEMORY_DMABUF;
        if (xioctl(_encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
            std::cout << "Failed to free output buffers" << std::endl;
        }
    }

    void V4l2Encoder::teardownDownstreamBuffers() {
        for (auto [index, buffer] : _downstream_buffers) {
            munmap(buffer->GetMemory(), buffer->GetSize());
            delete buffer;
        }
        v4l2_requestbuffers reqbufs = {};
        reqbufs.count = 0;
        reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        reqbufs.memory = V4L2_MEMORY_MMAP;
        if (xioctl(_encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
            std::cout << "Failed to free capture buffers" << std::endl;
        }
    }
}
//
// Created by brucegoose on 4/12/23.
//

#include "v4l2_decoder.hpp"


#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <filesystem>

#include <cstring>


#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;


int xioctl(int fd, unsigned long ctl, void *arg) {
    int ret, num_tries = 10;
    do
    {
        ret = ioctl(fd, ctl, arg);
    } while (ret == -1 && errno == EINTR && num_tries-- > 0);
    return ret;
}

namespace infrastructure {

    const char V4l2Decoder::_device_name[] = "/dev/video10";

    V4l2Decoder::V4l2Decoder(const DecoderConfig &config, DecoderBufferCallback output_callback):
        _output_callback(std::move(output_callback)),
        _width_height(config.get_decoder_width_height())
    {
        auto upstream_count = config.get_decoder_upstream_buffer_count();
        auto downstream_count = config.get_decoder_downstream_buffer_count();
        setupDecoder();
        setupUpstreamBuffers(upstream_count);
        setupDownstreamBuffers(downstream_count);
    }

    void V4l2Decoder::Start() {

        if (!_work_stop) {
            return;
        }

        int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        if (xioctl(_decoder_fd, VIDIOC_STREAMON, &type) < 0)
            throw std::runtime_error("failed to start output");

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (xioctl(_decoder_fd, VIDIOC_STREAMON, &type) < 0)
            throw std::runtime_error("failed to start capture");

        _is_primed = false;
        _work_stop = false;

        auto self(shared_from_this());
        _work_thread = std::make_unique<std::thread>([this, s = std::move(self)]() mutable { run(); });

    }

    void V4l2Decoder::Stop() {
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

        int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        if (xioctl(_decoder_fd, VIDIOC_STREAMOFF, &type) < 0)
            throw std::runtime_error("failed to stop output");

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (xioctl(_decoder_fd, VIDIOC_STREAMOFF, &type) < 0)
            throw std::runtime_error("failed to stop capture");

        /*
         * I think I should dequeue things but meh
         */

    }

    [[nodiscard]] std::shared_ptr<ResizableBuffer> V4l2Decoder::GetResizableBuffer() {
        V4l2ResizableBuffer *v4l2_rz_buffer = nullptr;
        {
            std::lock_guard<std::mutex> lock(_available_upstream_buffers_mutex);
            if (!_available_upstream_buffers.empty()) {
                v4l2_rz_buffer = _available_upstream_buffers.front();
                _available_upstream_buffers.pop();
            }
        }
        if (v4l2_rz_buffer == nullptr) {
            return _leaky_upstream_buffer;
        }

        // we use a capture with self here so the object isn't destructed if we have outstanding refs
        auto buffer = std::shared_ptr<ResizableBuffer>(v4l2_rz_buffer, [](ResizableBuffer *){});
        return buffer;
    }

    void V4l2Decoder::PostResizableBuffer(std::shared_ptr<ResizableBuffer> &&rz_buffer) {

        if (rz_buffer == nullptr || rz_buffer->IsLeakyBuffer()) {
            return;
        }

        auto v4l2_rz_buffer = std::static_pointer_cast<V4l2ResizableBuffer>(rz_buffer);
        if (_work_stop) {
            std::unique_lock<std::mutex> lock(_available_upstream_buffers_mutex);
            _available_upstream_buffers.push(v4l2_rz_buffer.get());
            return;
        }

        std::unique_lock<std::mutex> lock(_work_mutex);
        _work_queue.push(std::move(v4l2_rz_buffer));
        _work_cv.notify_one();

    }

    void V4l2Decoder::run() {
        while(!_work_stop) {
            std::shared_ptr<V4l2ResizableBuffer> buffer;
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
            decodeBuffer(std::move(buffer));
        }
    }

    void V4l2Decoder::decodeBuffer(std::shared_ptr<V4l2ResizableBuffer> &&v4l2_rz_buffer) {

        /*
         * dequeue output buffer, wait for it to finish
         */

        v4l2_plane planes[VIDEO_MAX_PLANES];
        v4l2_buffer buffer = {};
        buffer = {};
        memset(planes, 0, sizeof(planes));
        buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buffer.index = v4l2_rz_buffer->GetIndex();
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.length = 1;
        buffer.m.planes = planes;
        buffer.m.planes[0].bytesused = v4l2_rz_buffer->GetSize();
        if (xioctl(_decoder_fd, VIDIOC_QBUF, &buffer) < 0)
            throw std::runtime_error("failed to queue output buffer");

        if (!_is_primed) {
            if (xioctl(_decoder_fd, VIDIOC_DQBUF, &buffer) < 0)
                throw std::runtime_error("failed to queue output buffer");


            if (xioctl(_decoder_fd, VIDIOC_QBUF, &buffer) < 0)
                throw std::runtime_error("failed to queue output buffer");

            _is_primed = true;
        }

        if (!waitForDecoder()) {
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

        int ret = xioctl(_decoder_fd, VIDIOC_DQBUF, &buffer);
        if (ret == 0) {
            auto downstream_buffer = _downstream_buffers.at(buffer.index);
            auto self(shared_from_this());
            auto wrapped_buffer = std::shared_ptr<DecoderBuffer>(
                    downstream_buffer,
                    [this, s = std::move(self)](DecoderBuffer *d) {
                        queueDownstreamBuffer(d);
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
        ret = xioctl(_decoder_fd, VIDIOC_DQBUF, &buffer);
        if (ret == 0) {
            std::lock_guard<std::mutex> lock(_available_upstream_buffers_mutex);
            auto rq_buffer = _upstream_buffers.at(buffer.index);
            _available_upstream_buffers.push(rq_buffer);
        }

    }

    bool V4l2Decoder::waitForDecoder() {
        int attempts = 3;
        while (attempts > 0) {
            pollfd p = { _decoder_fd, POLLIN, 0 };
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

    void V4l2Decoder::queueDownstreamBuffer(DecoderBuffer *d) const {
        v4l2_buffer buf = {};
        v4l2_plane planes[VIDEO_MAX_PLANES] = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = d->GetIndex();
        buf.length = 1;
        buf.m.planes = planes;
        if (xioctl(_decoder_fd, VIDIOC_QBUF, &buf) < 0)
            throw std::runtime_error("failed to re-queue encoded buffer");
    }

    void V4l2Decoder::setupDecoder() {

        _decoder_fd = open(_device_name, O_RDWR, 0);

        v4l2_format fmt = {0};
        fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        fmt.fmt.pix_mp.width = _width_height.first;
        fmt.fmt.pix_mp.height = _width_height.second;
        fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MJPEG;
        fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
        fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;
        fmt.fmt.pix_mp.num_planes = 1;
        fmt.fmt.pix_mp.plane_fmt[0].bytesperline = 0;
        fmt.fmt.pix_mp.plane_fmt[0].sizeimage = 512 << 10;

        if (xioctl(_decoder_fd, VIDIOC_S_FMT, &fmt))
            throw std::runtime_error("failed to set output caps");


        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        fmt.fmt.pix_mp.width = _width_height.first;
        fmt.fmt.pix_mp.height = _width_height.second;
        fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420;
        fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
        fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;
        fmt.fmt.pix_mp.num_planes = 1;
        fmt.fmt.pix_mp.plane_fmt[0].bytesperline = _width_height.first;
        fmt.fmt.pix_mp.plane_fmt[0].sizeimage = _width_height.first * _width_height.second * 3 / 2;

        if (xioctl(_decoder_fd, VIDIOC_S_FMT, &fmt))
            throw std::runtime_error("failed to set capture caps");

    }

    void V4l2Decoder::setupUpstreamBuffers(const unsigned int request_upstream_buffers) {

        v4l2_requestbuffers reqbufs = {};
        reqbufs.count = request_upstream_buffers;
        reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        reqbufs.memory = V4L2_MEMORY_MMAP;
        if (xioctl(_decoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
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

            if (xioctl(_decoder_fd, VIDIOC_QUERYBUF, &buffer) < 0)
                throw std::runtime_error("failed to query output buffer");

            /*
             * mmap buffer
             */

            auto output_size = buffer.m.planes[0].length;
            auto output_offset = buffer.m.planes[0].m.mem_offset;
            auto output_mem = mmap(
                nullptr, output_size, PROT_READ | PROT_WRITE, MAP_SHARED, _decoder_fd, output_offset
            );
            if (output_mem == MAP_FAILED)
                throw std::runtime_error("failed to mmap output buffer");

            std::cout << "V4l2 Decoder MMAPed output buffer with size like so: " <<
                      output_size << ", " << output_offset << ", " << buffer.index <<
                      ", " << (void *) output_mem << std::endl;

            /*
             * Create proxy
             */

            auto upstream_buffer = new V4l2ResizableBuffer(output_mem, buffer.index, output_size);
            _available_upstream_buffers.push(upstream_buffer);
            _upstream_buffers.insert({buffer.index, upstream_buffer});
        }

        /*
         * Create upstream leaker
         */

        _leaky_upstream_buffer = std::make_shared<V4l2LeakyUpstreamBuffer>(
            _width_height.first * _width_height.second * 3 / 2
        );
    }

    void V4l2Decoder::setupDownstreamBuffers(unsigned int request_downstream_buffers) {

        v4l2_requestbuffers reqbufs = {};
        reqbufs.count = request_downstream_buffers;
        reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        reqbufs.memory = V4L2_MEMORY_MMAP;
        if (xioctl(_decoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
            throw std::runtime_error("request for output buffers failed");
        } else if (reqbufs.count != request_downstream_buffers) {
            std::stringstream out_str;
            out_str << "Unable to return " << request_downstream_buffers << " capture buffers; only got " << reqbufs.count;
            throw std::runtime_error(out_str.str());
        }

        v4l2_plane planes[VIDEO_MAX_PLANES];
        v4l2_buffer buffer = {};
        v4l2_exportbuffer expbuf = {};

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

            if (xioctl(_decoder_fd, VIDIOC_QUERYBUF, &buffer) < 0)
                throw std::runtime_error("failed to query output buffer");

            /*
             * mmap
             */

            auto capture_size = buffer.m.planes[0].length;
            auto capture_offset = buffer.m.planes[0].m.mem_offset;
            auto capture_mem = mmap(
                    nullptr, capture_size, PROT_READ | PROT_WRITE, MAP_SHARED, _decoder_fd, capture_offset
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

            if (xioctl(_decoder_fd, VIDIOC_EXPBUF, &expbuf) < 0)
                throw std::runtime_error("failed to export the capture buffer");

            /*
             * queue buffer
             */

            if (xioctl(_decoder_fd, VIDIOC_QBUF, &buffer) < 0)
                throw std::runtime_error("failed to dequeue capture buffer");

            /*
             * setup proxy
             */

            auto downstream_buffer = new DecoderBuffer(buffer.index, expbuf.fd, capture_mem, capture_size);
            _downstream_buffers.insert({ buffer.index, downstream_buffer });
        }
    }

    V4l2Decoder::~V4l2Decoder() {
        Stop();
        teardownUpstreamBuffers();
        teardownDownstreamBuffers();
        close(_decoder_fd);
    }

    void V4l2Decoder::teardownUpstreamBuffers() {
        for (auto [index, buffer] : _upstream_buffers) {
            munmap(buffer->GetMemory(), buffer->GetMaxSize());
        }

        v4l2_requestbuffers reqbufs = {};
        reqbufs.count = 0;
        reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        reqbufs.memory = V4L2_MEMORY_MMAP;
        if (xioctl(_decoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
            std::cout << "Failed to free output buffers" << std::endl;
        }
    }

    void V4l2Decoder::teardownDownstreamBuffers() {
        for (auto [index, buffer] : _downstream_buffers) {
            munmap(buffer->GetMemory(), buffer->GetSize());
            close(buffer->GetFd());
        }

        v4l2_requestbuffers reqbufs = {};
        reqbufs.count = 0;
        reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        reqbufs.memory = V4L2_MEMORY_MMAP;
        if (xioctl(_decoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
            std::cout << "Failed to free capture buffers" << std::endl;
        }
    }
}
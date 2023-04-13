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

#include <sstream>
#include <cstring>


#include <fstream>
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
        setupDecoder(upstream_count, downstream_count);
        setupUpstreamBuffers(upstream_count);
        setupDownstreamBuffers(downstream_count);
    }

    void V4l2Decoder::Dummy() {

        Start();

        std::filesystem::path this_dir = THIS_DIR;
        auto in_frame = this_dir;
        in_frame /= "in.jpeg";

        std::ifstream test_in_file(in_frame, std::ios::in | std::ios::binary);
        test_in_file.seekg(0, std::ios::end);
        size_t input_size = test_in_file.tellg();
        test_in_file.seekg(0, std::ios::beg);
        std::array<char, 1990656> in_buf = {};
        test_in_file.read(in_buf.data(), input_size);

        for (int i = 0; i < 300; i++) {
            auto buffer = GetResizableBuffer();
            auto v4l2_rz_buffer = std::static_pointer_cast<V4l2ResizableBuffer>(buffer);
            auto v4l2_ptr = v4l2_rz_buffer.get();
            buffer.reset();

            memcpy((void *)v4l2_ptr->GetMemory(), (void *) in_buf.data(), input_size);
            v4l2_ptr->SetSize(input_size);

            std::this_thread::sleep_for(30ms);

            v4l2_plane planes[VIDEO_MAX_PLANES];
            v4l2_buffer buf = {};
            buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            buf.index = v4l2_ptr->GetIndex();
            buf.memory = V4L2_MEMORY_MMAP;
            buf.length = 1;
            buf.m.planes = planes;
            buf.m.planes[0].bytesused = v4l2_ptr->GetSize();
            if (xioctl(_decoder_fd, VIDIOC_QBUF, &buf) < 0)
                throw std::runtime_error("failed to queue output buffer");

            if (i == 0) {
                if (xioctl(_decoder_fd, VIDIOC_DQBUF, &buf) < 0)
                    throw std::runtime_error("failed to queue output buffer");


                if (xioctl(_decoder_fd, VIDIOC_QBUF, &buf) < 0)
                    throw std::runtime_error("failed to queue output buffer");
                i++;
            }

            std::this_thread::sleep_for(30ms);
        }

        std::this_thread::sleep_for(1s);

        Stop();

    }

    void V4l2Decoder::Start() {

        if (_decoder_running) {
            return;
        }

        int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        if (xioctl(_decoder_fd, VIDIOC_STREAMON, &type) < 0)
            throw std::runtime_error("failed to start output");

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (xioctl(_decoder_fd, VIDIOC_STREAMON, &type) < 0)
            throw std::runtime_error("failed to start capture");

        _is_primed = false;
        _decoder_running = true;

        auto self(shared_from_this());
        _downstream_thread = std::make_unique<std::thread>([this, s = std::move(self)]() mutable {
            handleDownstream();
        });

    }

    void V4l2Decoder::Stop() {
        if (!_decoder_running) {
            return;
        }
        _decoder_running = false;

        if (_downstream_thread) {
            if (_downstream_thread->joinable()) {
                _downstream_thread->join();
            }
            _downstream_thread.reset();
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
        V4l2ResizableBuffer *v4l2_resizable_buffer;
        {
            std::lock_guard<std::mutex> lock(_available_upstream_buffers_mutex);
            v4l2_resizable_buffer = _available_upstream_buffers.front();
            if (v4l2_resizable_buffer) {
                _available_upstream_buffers.pop();
            }
        }
        if (!v4l2_resizable_buffer) {
            std::cout << "returning leaky" << std::endl;
            return _leaky_upstream_buffer;
        }
        std::cout << v4l2_resizable_buffer->GetIndex() << ", " << v4l2_resizable_buffer->GetMemory() << std::endl;
        // we use a capture with self here so the object isn't destructed if we have outstanding refs
        auto self(shared_from_this());
        auto buffer = std::shared_ptr<V4l2ResizableBuffer>(
            v4l2_resizable_buffer, [](V4l2ResizableBuffer *) {
                std::cout << "ima deconstructing boy" << std::endl;
            }
        );
        return std::move(buffer);
    }

    void V4l2Decoder::PostResizableBuffers(V4l2ResizableBuffer *v4l2_ptr) {

        static int i = 0;

        v4l2_plane planes[VIDEO_MAX_PLANES];
        v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buf.index = v4l2_ptr->GetIndex();
        buf.memory = V4L2_MEMORY_MMAP;
        buf.length = 1;
        buf.m.planes = planes;
        buf.m.planes[0].bytesused = v4l2_ptr->GetSize();
        if (xioctl(_decoder_fd, VIDIOC_QBUF, &buf) < 0)
            throw std::runtime_error("failed to queue output buffer");

        if (i == 0) {
            if (xioctl(_decoder_fd, VIDIOC_DQBUF, &buf) < 0)
                throw std::runtime_error("failed to queue output buffer");


            if (xioctl(_decoder_fd, VIDIOC_QBUF, &buf) < 0)
                throw std::runtime_error("failed to queue output buffer");
            i++;
        }
    }

    void V4l2Decoder::PostResizableBuffer(std::shared_ptr<ResizableBuffer> &&rz_buffer) {

        /*
        auto upstream_buffer = (V4l2UpstreamBuffer *) rz_buffer.get();

        if (upstream_buffer->IsLeakyBuffer()) {
            return;
        }

        auto v4l2_rz_buffer = (V4l2ResizableBuffer *) upstream_buffer;

        if (!_decoder_running) {
            std::unique_lock<std::mutex> lock(_available_upstream_buffers_mutex);
            _available_upstream_buffers.push(v4l2_rz_buffer);
            return;
        }

         */

        auto v4l2_rz_buffer = std::static_pointer_cast<V4l2ResizableBuffer>(rz_buffer);

        std::cout << v4l2_rz_buffer->GetMemory() << ", " << v4l2_rz_buffer->GetSize() << ", " << v4l2_rz_buffer->GetIndex() << std::endl;

        const unsigned char *bytes = static_cast<const unsigned char *>(v4l2_rz_buffer->GetMemory());
        for (int i = 0; i < 5; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]) << ' ';
        }
        std::cout << std::endl;

        v4l2_plane planes[VIDEO_MAX_PLANES];
        v4l2_buffer buf = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buf.index = v4l2_rz_buffer->GetIndex();
        buf.memory = V4L2_MEMORY_MMAP;
        buf.length = 1;
        buf.m.planes = planes;
        buf.m.planes[0].bytesused = v4l2_rz_buffer->GetSize();
        if (xioctl(_decoder_fd, VIDIOC_QBUF, &buf) < 0)
            throw std::runtime_error("failed to queue output buffer");


        if (!_is_primed) {
            if (xioctl(_decoder_fd, VIDIOC_DQBUF, &buf) < 0)
                throw std::runtime_error("failed to dequeue output buffer primer");

            if (xioctl(_decoder_fd, VIDIOC_QBUF, &buf) < 0)
                throw std::runtime_error("failed to queue output buffer during priming");

            _is_primed = true;
        }

    }

    void V4l2Decoder::handleDownstream() {
        while (_decoder_running) {
            const auto decoder_ready = waitForDecoder();
            {
                std::lock_guard<std::mutex> lock(_available_upstream_buffers_mutex);
                if (_available_upstream_buffers.size() == _upstream_buffers.size()) {
                    continue;
                }
            }
            if (!_decoder_running) {
                break;
            } else if (!decoder_ready) {
                continue;
            }
            auto downstream_buffer = getDownstreamBuffer();
            if (downstream_buffer) {
                _output_callback(std::move(downstream_buffer));
            }
        }
    }

    bool V4l2Decoder::waitForDecoder() {
        int attempts = 5;
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

    std::shared_ptr<DecoderBuffer> V4l2Decoder::getDownstreamBuffer() {

        /*
         * dequeue upstream buffer, return it to the pool
         */

        v4l2_buffer buf = {};
        v4l2_plane planes[VIDEO_MAX_PLANES] = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.length = 1;
        buf.m.planes = planes;
        int ret = xioctl(_decoder_fd, VIDIOC_DQBUF, &buf);
        if (ret == 0) {
            std::lock_guard<std::mutex> lock(_available_upstream_buffers_mutex);
            auto v4l2_rz_buffer = _upstream_buffers.at(buf.index);
            _available_upstream_buffers.push(v4l2_rz_buffer);
        }
        /*
         * dequeue downstream buffer; wrap it and return it
         */

        buf = {};
        memset(planes, 0, sizeof(planes));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.length = 1;
        buf.m.planes = planes;
        ret = xioctl(_decoder_fd, VIDIOC_DQBUF, &buf);
        if (ret == 0) {
            auto downstream_buffer = _downstream_buffers.at(buf.index);
            auto self(shared_from_this());
            auto wrapped_buffer = std::shared_ptr<DecoderBuffer>(
                downstream_buffer,
                [this, s = std::move(self)](DecoderBuffer *d) {
                    queueDownstreamBuffer(d);
                }
            );
            return std::move(wrapped_buffer);
        } else {
            return nullptr;
        }
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

    void V4l2Decoder::setupDecoder(unsigned int request_upstream_buffers, unsigned int request_downstream_buffers) {

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

        reqbufs = {};
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


    }

    void V4l2Decoder::setupUpstreamBuffers(const unsigned int request_upstream_buffers) {

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
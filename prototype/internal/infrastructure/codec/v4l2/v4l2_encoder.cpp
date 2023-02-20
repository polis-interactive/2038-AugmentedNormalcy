//
// Created by brucegoose on 2/5/23.
//

#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>

#include <linux/videodev2.h>

#include "v4l2_encoder.hpp"


namespace Codec {

    void V4l2Encoder::StartEncoder() {
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        if (xioctl(_encoder_fd, VIDIOC_STREAMON, &type) < 0)
            throw std::runtime_error("failed to start output streaming");
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (xioctl(_encoder_fd, VIDIOC_STREAMON, &type) < 0)
            throw std::runtime_error("failed to start capture streaming");
        std::cout << "H264 encoder started" << std::endl;
        if (!_downstream_thread) {
            _downstream_thread = std::make_unique<std::jthread>(std::bind_front(&V4l2Encoder::HandleDownstream, this));
        }
    }

    void V4l2Encoder::DoStopEncoder() {
        if (_downstream_thread) {
            if (_downstream_thread->joinable()) {
                _downstream_thread->request_stop();
                _downstream_thread->join();
            }
            _downstream_thread.reset();
        }
    }

    int V4l2Encoder::xioctl(int fd, unsigned long ctl, void *arg) {
        int ret, num_tries = 10;
        do
        {
            ret = ioctl(fd, ctl, arg);
        } while (ret == -1 && errno == EINTR && num_tries-- > 0);
        return ret;
    }

    void V4l2Encoder::CreateEncoder(const Config &config, std::shared_ptr<Context> &context) {

#if _AN_PLATFORM_ == PLATFORM_RPI
        const char device_name[] = "/dev/video11";
#else
        // this is probably different on the jetson... hell it just fails on my linux laptop
        const char device_name[] = "/dev/video11";
#endif
        _encoder_fd = open(device_name, O_RDWR, 0);
        if (_encoder_fd < 0) {
            std::cerr << "failed to open V4L2 H264 encoder" << std::endl;
            throw std::runtime_error("failed to open V4L2 H264 encoder");
        }
        std::cout << "Opened H264Encoder on " << device_name << " as fd " << _encoder_fd << std::endl;

        SetupEncoder(config);
        SetupBuffers(config.get_camera_buffer_count(), config.get_encoder_buffer_count());
    }

    // ctrls from here: https://www.kernel.org/doc/html/v5.15/userspace-api/media/v4l/ext-ctrls-codec.html?highlight=v4l2_cid_mpeg_video_h264_profile
    // v4l2-ctl -d /dev/video11 --list-ctrls
    void V4l2Encoder::SetupEncoder(const Codec::Config &config) {

        // for now, we are hardcoding this. W.e.
        const int width = 1536;
        const int height = 864;
        const int stride = 1536;

        v4l2_control ctrl = {};

        // set profile
        ctrl.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
        ctrl.value = V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;
        if (xioctl(_encoder_fd, VIDIOC_S_CTRL, &ctrl) < 0)
            throw std::runtime_error("failed to set profile");

        // set level: no idea what this does; I think its the same as preset
        ctrl.id = V4L2_CID_MPEG_VIDEO_H264_LEVEL;
        ctrl.value = V4L2_MPEG_VIDEO_H264_LEVEL_4_0;
        if (xioctl(_encoder_fd, VIDIOC_S_CTRL, &ctrl) < 0)
            throw std::runtime_error("failed to set level");

        // set cbr
        ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE_MODE;
        ctrl.value = V4L2_MPEG_VIDEO_BITRATE_MODE_CBR;
        if (xioctl(_encoder_fd, VIDIOC_S_CTRL, &ctrl) < 0)
            throw std::runtime_error("failed to set bitrate");

        // set average bitrate
        ctrl.id = V4L2_CID_MPEG_VIDEO_BITRATE;
        ctrl.value = (static_cast<unsigned int>(5.0f * width * height) / (1280 * 720)) * 100000;
        if (xioctl(_encoder_fd, VIDIOC_S_CTRL, &ctrl) < 0)
            throw std::runtime_error("failed to set bitrate");

        // set infinite gop

        ctrl.id = V4L2_CID_MPEG_VIDEO_H264_I_PERIOD;
        ctrl.value = 2147483647;
        if (xioctl(_encoder_fd, VIDIOC_S_CTRL, &ctrl) < 0)
            throw std::runtime_error("failed to set bitrate");

        v4l2_format fmt = {};
        fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        fmt.fmt.pix_mp.width = width;
        fmt.fmt.pix_mp.height = height;
        // We assume YUV420 here, but it would be nice if we could do something
        // like info.pixel_format.toV4L2Fourcc();
        fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420;
        fmt.fmt.pix_mp.plane_fmt[0].bytesperline = stride;
        fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
        // this may need to be rec601...
        fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;
        fmt.fmt.pix_mp.num_planes = 1;
        if (xioctl(_encoder_fd, VIDIOC_S_FMT, &fmt) < 0)
            throw std::runtime_error("failed to set output format");

        fmt = {};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        fmt.fmt.pix_mp.width = width;
        fmt.fmt.pix_mp.height = height;
        fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
        fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
        fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_DEFAULT;
        fmt.fmt.pix_mp.num_planes = 1;
        fmt.fmt.pix_mp.plane_fmt[0].bytesperline = 0;
        fmt.fmt.pix_mp.plane_fmt[0].sizeimage = 512 << 10;
        if (xioctl(_encoder_fd, VIDIOC_S_FMT, &fmt) < 0)
            throw std::runtime_error("failed to set capture format");

        struct v4l2_streamparm parm = {};
        parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        parm.parm.output.timeperframe.numerator = 90000 / config.get_fps();
        parm.parm.output.timeperframe.denominator = 90000;
        if (xioctl(_encoder_fd, VIDIOC_S_PARM, &parm) < 0)
            throw std::runtime_error("failed to set streamparm");
    }

    void V4l2Encoder::SetupBuffers(const int camera_buffer_count, const int downstream_buffers_count) {

        // buffers passed from the camera
        v4l2_requestbuffers reqbufs = {};
        reqbufs.count = camera_buffer_count;
        reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        reqbufs.memory = V4L2_MEMORY_DMABUF;
        if (xioctl(_encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
            throw std::runtime_error("request for output buffers failed");
        }
        else if (reqbufs.count < camera_buffer_count) {
            throw std::runtime_error("Couldn't allocate all camera buffers");
        }
        // available camera buffers to encode
        for (uint32_t i = 0; i < reqbufs.count; i++)
            _input_buffers_available.push(i);

        // buffers from downstream
        reqbufs = {};
        reqbufs.count = downstream_buffers_count;
        reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        reqbufs.memory = V4L2_MEMORY_MMAP;
        if (xioctl(_encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
            throw std::runtime_error("request for capture buffers failed");
        }
        else if (reqbufs.count < downstream_buffers_count) {
            throw std::runtime_error("Couldn't allocate all downstream buffers");
        }

        _downstream_buffers.resize(reqbufs.count);

        for (unsigned int i = 0; i < reqbufs.count; i++)
        {
            v4l2_plane planes[VIDEO_MAX_PLANES];
            v4l2_buffer buffer = {};
            buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            buffer.memory = V4L2_MEMORY_MMAP;
            buffer.index = i;
            buffer.length = 1;
            buffer.m.planes = planes;
            if (xioctl(_encoder_fd, VIDIOC_QUERYBUF, &buffer) < 0)
                throw std::runtime_error("failed to capture query buffer " + std::to_string(i));
            auto b = std::make_shared<BufferDescription>();
            b->size = buffer.m.planes[0].length;
            b->index = i;
            b->mem = mmap(
                nullptr, buffer.m.planes[0].length, PROT_READ | PROT_WRITE, MAP_SHARED, _encoder_fd,
                buffer.m.planes[0].m.mem_offset
            );
            std::cout << "Allocated mem: " << b->mem << std::endl;
            if (b->mem == MAP_FAILED)
                throw std::runtime_error("failed to mmap capture buffer " + std::to_string(i));

            _downstream_buffers[i] = b;
            // Whilst we're going through all the capture buffers, we may as well queue
            // them ready for the encoder to write into.
            if (xioctl(_encoder_fd, VIDIOC_QBUF, &buffer) < 0)
                throw std::runtime_error("failed to queue capture buffer " + std::to_string(i));
        }

        std::cout << "V4l2 Encoder made buffers" << std::endl;
    }

    void V4l2Encoder::TryEncode(std::shared_ptr<void> &&buffer) {
        uint32_t index;
        {
            std::lock_guard<std::mutex> lock(_input_buffers_available_mutex);
            if (_input_buffers_available.empty())
                throw std::runtime_error("no buffers available to queue codec input");
            index = _input_buffers_available.front();
            _input_buffers_available.pop();
        }
        auto input_buffer = std::static_pointer_cast<CameraBuffer>(buffer);
        v4l2_buffer buf = {};
        v4l2_plane planes[VIDEO_MAX_PLANES] = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buf.index = index;
        buf.field = V4L2_FIELD_NONE;
        buf.memory = V4L2_MEMORY_DMABUF;
        buf.length = 1;
        buf.m.planes = planes;
        buf.m.planes[0].m.fd = input_buffer->GetFd();
        buf.m.planes[0].bytesused = input_buffer->GetSize();
        buf.m.planes[0].length = input_buffer->GetSize();
        auto ret = xioctl(_encoder_fd, VIDIOC_QBUF, &buf);
        if (ret < 0) {
            std::cout << "Encountered ret: " << ret << std::endl;
            throw std::runtime_error("failed to queue input to codec");
        }
        {
            // we could use a map, but let's just assume they happen in order;
            // good enough for pi people, good enough for me!
            std::lock_guard<std::mutex> lock(_input_buffers_processing_mutex);
            _input_buffers_processing.push(std::move(buffer));
        }
    }


    void V4l2Encoder::HandleDownstream(std::stop_token st) noexcept {
        while (true) {
            const auto encoder_ready = WaitForEncoder();
            if (st.stop_requested()) {
                break;
            } else if (!encoder_ready) {
                continue;
            }
            auto downstream_buffer = GetDownstreamBuffer();
            if (!downstream_buffer) {
                // pretty sure we are actually fkd here, need a way to recover...
                continue;
            }
            if (downstream_buffer->bytes_used != 0) {
                SendDownstreamBuffer(downstream_buffer);
            }
            QueueDownstreamBuffer(downstream_buffer);
        }
    }

    bool V4l2Encoder::WaitForEncoder() {
        int attempts = 3;
        while (attempts > 0) {
            pollfd p = { _encoder_fd, POLLIN, 0 };
            int ret = poll(&p, 1, 10);
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

    std::shared_ptr<V4l2Encoder::BufferDescription> &V4l2Encoder::GetDownstreamBuffer() {
        v4l2_buffer buf = {};
        v4l2_plane planes[VIDEO_MAX_PLANES] = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buf.memory = V4L2_MEMORY_DMABUF;
        buf.length = 1;
        buf.m.planes = planes;
        int ret = xioctl(_encoder_fd, VIDIOC_DQBUF, &buf);
        if (ret == 0) {
            {
                std::lock_guard<std::mutex> lock(_input_buffers_available_mutex);
                _input_buffers_available.push(buf.index);
            }
            {
                // me thinks this is good enough; pop should dereference the smart pointer can call its callback,
                // which should return it to the camera pool
                std::lock_guard<std::mutex> lock(_input_buffers_processing_mutex);
                _input_buffers_processing.pop();
            }
        }

        buf = {};
        memset(planes, 0, sizeof(planes));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.length = 1;
        buf.m.planes = planes;
        ret = xioctl(_encoder_fd, VIDIOC_DQBUF, &buf);
        if (ret == 0) {
            auto &downstream_buffer = _downstream_buffers.at(buf.index);
            downstream_buffer->bytes_used = buf.m.planes[0].bytesused;
            return downstream_buffer;
        }
        return _dummy_out;
    }

    void V4l2Encoder::SendDownstreamBuffer(std::shared_ptr<BufferDescription> &downstream_buffer) {
        auto output_buffer = _b_pool.New();
        std::cout << "Writing: " << output_buffer->GetMemory() << std::endl;
        std::cout << "From: " << downstream_buffer->mem << std::endl;
        std::memcpy(
            (uint8_t *)output_buffer->GetMemory(),
            (uint8_t *)downstream_buffer->mem,
            downstream_buffer->bytes_used
        );
        _sequence_number += 1;
        BspPacket packet{};
        packet.session_number = _session_number;
        packet.sequence_number = _sequence_number;
        packet.Pack(output_buffer->_buffer.data(), downstream_buffer->bytes_used);
        output_buffer->_size = downstream_buffer->bytes_used + BspPacket::HeaderSize();
        auto out_buffer = std::shared_ptr<void>(output_buffer.get(), [this, &output_buffer](void *b_ptr) mutable {
            std::cout << "Freed: " << output_buffer->GetMemory();
            _b_pool.Free(std::move(output_buffer));
        });
        _send_callback(std::move(out_buffer));
    }

    void V4l2Encoder::QueueDownstreamBuffer(std::shared_ptr<BufferDescription> &downstream_buffer) {
        v4l2_buffer buf = {};
        v4l2_plane planes[VIDEO_MAX_PLANES] = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = downstream_buffer->index;
        buf.length = 1;
        buf.m.planes = planes;
        buf.m.planes[0].bytesused = 0;
        buf.m.planes[0].length = downstream_buffer->size;
        if (xioctl(_encoder_fd, VIDIOC_QBUF, &buf) < 0)
            throw std::runtime_error("failed to re-queue encoded buffer");
    }

    V4l2Encoder::~V4l2Encoder() {
        DoStopEncoder();

        // stop streaming
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        if (xioctl(_encoder_fd, VIDIOC_STREAMOFF, &type) < 0) {
            std::cout << "Failed to stop output streaming" << std::endl;
        }
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (xioctl(_encoder_fd, VIDIOC_STREAMOFF, &type) < 0) {
            std::cout << "Failed to stop capture streaming" << std::endl;
        }

        v4l2_requestbuffers reqbufs = {};
        reqbufs.count = 0;
        reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        reqbufs.memory = V4L2_MEMORY_DMABUF;
        if (xioctl(_encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
            std::cout << "Failed to free output buffers" << std::endl;
        }

        for (auto &downstream_buffer : _downstream_buffers) {
            if (munmap(downstream_buffer->mem, downstream_buffer->size) < 0) {
                std::cout << "Failed to unmap buffer" << std::endl;
            }
        }

        reqbufs = {};
        reqbufs.count = 0;
        reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        reqbufs.memory = V4L2_MEMORY_MMAP;
        if (xioctl(_encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
            std::cout << "Failed to free capture buffers" << std::endl;
        }

        close(_encoder_fd);
        std::cout << "V4l2 Encoder Closed" << std::endl;
    }

}
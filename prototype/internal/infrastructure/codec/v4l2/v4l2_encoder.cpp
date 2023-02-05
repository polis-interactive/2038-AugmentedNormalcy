//
// Created by brucegoose on 2/5/23.
//

#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>

#include <linux/videodev2.h>
#include "preprocessor.hpp"

#include "v4l2_encoder.hpp"


namespace Codec {

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
        std::cout << "Opened H264Encoder on " << device_name << " as fd " << _encoder_fd;

        SetupEncoder(config);
        SetupBuffers();
    }

    // ctrls from here: https://www.kernel.org/doc/html/v5.15/userspace-api/media/v4l/ext-ctrls-codec.html?highlight=v4l2_cid_mpeg_video_h264_profile
    // v4l2-ctl -d /dev/video11 --list-ctrls
    void V4l2Encoder::SetupEncoder(const Codec::Config &config) {

        // for now, we are hardcoding this. W.e.
        const int width = 1296;
        const int height = 728;
        const int stride = 1344;

        v4l2_control ctrl = {};

        // set profile
        ctrl.id = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
        ctrl.value = V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE;
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
        parm.parm.output.timeperframe.numerator = 1000 / config.get_fps();
        parm.parm.output.timeperframe.denominator = 1000;
        if (xioctl(_encoder_fd, VIDIOC_S_PARM, &parm) < 0)
            throw std::runtime_error("failed to set streamparm");
    }

    void V4l2Encoder::SetupBuffers() {

        // buffers from downstream
        v4l2_requestbuffers reqbufs = {};
        reqbufs.count = NUM_CAPTURE_BUFFERS;
        reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        reqbufs.memory = V4L2_MEMORY_MMAP;
        if (xioctl(_encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0)
            throw std::runtime_error("request for capture buffers failed");
        std::cout << "Got " << reqbufs.count << " downstream buffers";
        _upstream_buffers_count = reqbufs.count;


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

            _upstream_buffers[i].mem = mmap(
                nullptr, buffer.m.planes[0].length + BspPacket::HeaderSize(), PROT_READ | PROT_WRITE, MAP_SHARED, _encoder_fd,
                buffer.m.planes[0].m.mem_offset
            );
            if (_upstream_buffers[i].mem == MAP_FAILED)
                throw std::runtime_error("failed to mmap capture buffer " + std::to_string(i));
            _upstream_buffers[i].size = buffer.m.planes[0].length + BspPacket::HeaderSize();
            // Whilst we're going through all the capture buffers, we may as well queue
            // them ready for the encoder to write into.
            if (xioctl(_encoder_fd, VIDIOC_QBUF, &buffer) < 0)
                throw std::runtime_error("failed to queue capture buffer " + std::to_string(i));
        }
    }

    std::size_t V4l2Encoder::EncodeFrame(void *packet_ptr, std::size_t header_size) {
        if (!WaitForEncoder()) {
            return 0;
        }
        v4l2_buffer buf = {};
        v4l2_plane planes[VIDEO_MAX_PLANES] = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buf.memory = V4L2_MEMORY_DMABUF;
        buf.length = 1;
        buf.m.planes = planes;
        xioctl(_encoder_fd, VIDIOC_DQBUF, &buf);
        // return frame to caller here somehow...?
        buf = {};
        memset(planes, 0, sizeof(planes));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.length = 1;
        buf.m.planes = planes;
        buf.m.offset = header_size;
        int ret = xioctl(_encoder_fd, VIDIOC_DQBUF, &buf);
        if (ret == 0) {

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

}
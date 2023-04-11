

#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>

#include <linux/videodev2.h>

#include <filesystem>
#include <cstring>
#include <fstream>


int xioctl(int fd, unsigned long ctl, void *arg) {
    int ret, num_tries = 10;
    do
    {
        ret = ioctl(fd, ctl, arg);
    } while (ret == -1 && errno == EINTR && num_tries-- > 0);
    return ret;
}

bool PollFd(int fd) {
    int attempts = 3;
    while (attempts > 0) {
        pollfd p = { fd, POLLIN, 0 };
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

int main(int argc, char *argv[]) {
    const char device_name[] = "/dev/video10";
    const int width = 1536;
    const int height = 864;
    const int stride = 1536;


    std::filesystem::path this_dir = TMP_DIR;

    auto in_frame = this_dir;
    in_frame /= "in.jpeg";

    auto out_frame = this_dir;
    out_frame /= "out.yuv";

    if(std::filesystem::remove(out_frame)) {
        std::cout << "Removed output file" << std::endl;
    } else {
        std::cout << "No output file to remove" << std::endl;
    }

    std::ifstream test_in_file(in_frame, std::ios::in | std::ios::binary);
    test_in_file.seekg(0, std::ios::end);
    size_t input_size = test_in_file.tellg();
    test_in_file.seekg(0, std::ios::beg);
    std::array<char, 1990656> in_buf = {};
    test_in_file.read(in_buf.data(), input_size);
    std::array<char, 1990656> out_buf = {};


    auto decoder_fd = open(device_name, O_RDWR, 0);
    if (decoder_fd == -1) {
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
        return 1;
    }
    std::cout << "V4l2 Decoder opened fd: " << decoder_fd << std::endl;

    /*
     *  check caps
     */

    v4l2_fmtdesc fmtdesc{0};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    for (int i = 0;; ++i) {
        fmtdesc.index = i;
        if (xioctl(decoder_fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1) {
            break;
        }
        std::cout << "Format " << i << ": " << fmtdesc.description
                  << ", FourCC: " << static_cast<char>((fmtdesc.pixelformat >> 0) & 0xFF)
                  << static_cast<char>((fmtdesc.pixelformat >> 8) & 0xFF)
                  << static_cast<char>((fmtdesc.pixelformat >> 16) & 0xFF)
                  << static_cast<char>((fmtdesc.pixelformat >> 24) & 0xFF)
                  << std::endl;
    }

    /*
     *  SET FORMATS
     */


    v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt.fmt.pix_mp.width = width;
    fmt.fmt.pix_mp.height = height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;
    fmt.fmt.pix_mp.num_planes = 1;


    if (xioctl(decoder_fd, VIDIOC_S_FMT, &fmt))
        throw std::runtime_error("failed to set output caps");


    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = width;
    fmt.fmt.pix_mp.height = height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420;
    fmt.fmt.pix_mp.plane_fmt[0].bytesperline = stride;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;
    fmt.fmt.pix_mp.num_planes = 1;

    if (xioctl(decoder_fd, VIDIOC_S_FMT, &fmt))
        throw std::runtime_error("failed to set output caps");

    std::cout << "V4l2 Decoder setup caps" << std::endl;

    /*
     * REQUEST BUFFERS
     */

    v4l2_requestbuffers reqbufs = {};
    reqbufs.count = 4;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbufs.memory = V4L2_MEMORY_DMABUF;
    if (xioctl(decoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
        std::cout << errno << std::endl;
        throw std::runtime_error("request for output buffers failed");
    }
    std::cout << "V4L2 Decoder got " << reqbufs.count << " output buffers" << std::endl;

    reqbufs = {};
    reqbufs.count = 4;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = V4L2_MEMORY_DMABUF;
    if (xioctl(decoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
        std::cout << errno << std::endl;
        throw std::runtime_error("request for capture buffers failed");
    }
    std::cout << "V4L2 Decoder got " << reqbufs.count << " capture buffers" << std::endl;

    /*
     * SETUP CAPTURE BUFFERS
     */

    v4l2_plane planes[VIDEO_MAX_PLANES];
    v4l2_buffer buffer = {};
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buffer.memory = V4L2_MEMORY_DMABUF;
    buffer.index = 0;
    buffer.length = 1;
    buffer.m.planes = planes;
    if (xioctl(decoder_fd, VIDIOC_QUERYBUF, &buffer) < 0)
        throw std::runtime_error("failed to query capture buffer");


    /*
     * SETUP OUTPUT BUFFERS
     */

    buffer = {};
    memset(planes, 0, sizeof(planes));
    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buffer.memory = V4L2_MEMORY_DMABUF;
    buffer.index = 0;
    buffer.length = 1;
    buffer.m.planes = planes;

    if (xioctl(decoder_fd, VIDIOC_QUERYBUF, &buffer) < 0)
        throw std::runtime_error("failed to query output buffer");

    std::cout << buffer.m.planes[0].m.fd << ", " << buffer.m.planes[0].length << std::endl;



    std::cout << "V4l2 Decoder stopped" << std::endl;

    close(decoder_fd);
    std::cout << "V4l2 Decoder Closed" << std::endl;
    return 0;
}
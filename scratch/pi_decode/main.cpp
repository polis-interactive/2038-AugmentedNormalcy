

#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>

#include <linux/videodev2.h>

#include <filesystem>
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
    const char device_name[] = "/dev/video11";
    const int width = 1536;
    const int height = 864;


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
        return 1;
    }
    std::cout << "V4l2 Decoder opened fd: " << encoder_fd << std::endl;

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt.fmt.pix_mp.width = width;
    fmt.fmt.pix_mp.height = height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MJPEG;

    if (xioctl(encoder_fd, VIDIOC_S_FMT, &fmt))
        throw std::runtime_error("failed to set output caps");


    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = width;
    fmt.fmt.pix_mp.height = height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420;

    if (xioctl(encoder_fd, VIDIOC_S_FMT, &fmt))
        throw std::runtime_error("failed to set output caps");

    std::cout << "V4l2 Decoder setup caps" << std::endl;

    struct v4l2_buffer buf = {0};
    struct v4l2_plane planes[1];
    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buf.memory = V4L2_MEMORY_USERPTR;
    buf.index = 0;
    buf.m.planes = planes;
    buf.length = 1;
    buf.m.planes[0].m.userptr = reinterpret_cast<unsigned long>(in_buf.data());
    buf.m.planes[0].length = input_size;
    buf.m.planes[0].bytesused = input_size;

    if (xioctl(encoder_fd, VIDIOC_QBUF, &buf)) {
        std::cout << errno << std::endl;
        throw std::runtime_error("failed to queue output buffer");
    }

    buf = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_USERPTR;
    buf.m.userptr = reinterpret_cast<unsigned long>(out_buf.data());
    buf.length = out_buf.size();
    if (xioctl(encoder_fd, VIDIOC_QBUF, &buf))
        throw std::runtime_error("failed to queue capture buffer");

    std::cout << "V4l2 Decoder queued buffers" << std::endl;

    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (xioctl(encoder_fd, VIDIOC_STREAMON, &buf.type))
        throw std::runtime_error("failed to start output processing");

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (xioctl(encoder_fd, VIDIOC_STREAMON, &buf.type))
        throw std::runtime_error("failed to start capture processing");

    std::cout << "V4l2 Decoder started" << std::endl;

    bool did_end = PollFd(encoder_fd);
    if (!did_end) {
        std::cerr << "Failed to poll fd" << std::endl;
    } else {
        std::cout << "SUCCESS!" << std::endl;
    }


    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (xioctl(encoder_fd, VIDIOC_STREAMOFF, &buf.type) < 0) {
        std::cout << "Failed to stop output streaming" << std::endl;
    }

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (xioctl(encoder_fd, VIDIOC_STREAMOFF, &buf.type) < 0)
        throw std::runtime_error("failed to start capture streaming");

    std::cout << "V4l2 Decoder stopped" << std::endl;

    close(encoder_fd);
    std::cout << "V4l2 Decoder Closed" << std::endl;
    return 0;
}
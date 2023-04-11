

#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>

#include <linux/videodev2.h>

int xioctl(int fd, unsigned long ctl, void *arg) {
    int ret, num_tries = 10;
    do
    {
        ret = ioctl(fd, ctl, arg);
    } while (ret == -1 && errno == EINTR && num_tries-- > 0);
    return ret;
}

int main(int argc, char *argv[]) {
    const char device_name[] = "/dev/video11";
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
    std::cout << "V4l2 Decoder opend fd: " << encoder_fd << std::endl;

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (xioctl(encoder_fd, VIDIOC_STREAMON, &type) < 0)
        throw std::runtime_error("failed to start output streaming");

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (xioctl(encoder_fd, VIDIOC_STREAMON, &type) < 0)
        throw std::runtime_error("failed to start capture streaming");

    std::cout << "V4l2 Decoder started" << std::endl;

    type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (xioctl(encoder_fd, VIDIOC_STREAMOFF, &type) < 0) {
        std::cout << "Failed to stop output streaming" << std::endl;
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (xioctl(encoder_fd, VIDIOC_STREAMOFF, &type) < 0)
        throw std::runtime_error("failed to start capture streaming");

    std::cout << "V4l2 Decoder stopped" << std::endl;

    close(encoder_fd);
    std::cout << "V4l2 Decoder Closed" << std::endl;
    return 0;
}
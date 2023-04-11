

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
     *  check formats
     */

    v4l2_fmtdesc fmtdesc{0};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    for (int i = 0;; ++i) {
        fmtdesc.index = i;
        if (xioctl(decoder_fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1) {
            break;
        }
        std::cout << "OUTPUT Format " << i << ": " << fmtdesc.description
                  << ", FourCC: " << static_cast<char>((fmtdesc.pixelformat >> 0) & 0xFF)
                  << static_cast<char>((fmtdesc.pixelformat >> 8) & 0xFF)
                  << static_cast<char>((fmtdesc.pixelformat >> 16) & 0xFF)
                  << static_cast<char>((fmtdesc.pixelformat >> 24) & 0xFF)
                  << std::endl;
    }

    fmtdesc = {};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    for (int i = 0;; ++i) {
        fmtdesc.index = i;
        if (xioctl(decoder_fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1) {
            break;
        }
        std::cout << "CAPTURE Format " << i << ": " << fmtdesc.description
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
    fmt.fmt.pix_mp.plane_fmt[0].bytesperline = 0;
    fmt.fmt.pix_mp.plane_fmt[0].sizeimage = 512 << 10;

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
    reqbufs.count = 1;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    if (xioctl(decoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
        std::cout << errno << std::endl;
        throw std::runtime_error("request for output buffers failed");
    }
    std::cout << "V4L2 Decoder got " << reqbufs.count << " output buffers" << std::endl;

    reqbufs = {};
    reqbufs.count = 1;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    if (xioctl(decoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
        std::cout << errno << std::endl;
        throw std::runtime_error("request for capture buffers failed");
    }
    std::cout << "V4L2 Decoder got " << reqbufs.count << " capture buffers" << std::endl;


    /*
     * SETUP OUTPUT BUFFERS
     */

    v4l2_plane planes[VIDEO_MAX_PLANES];
    v4l2_buffer buffer = {};

    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = 0;
    buffer.length = 1;
    buffer.m.planes = planes;

    if (xioctl(decoder_fd, VIDIOC_QUERYBUF, &buffer) < 0)
        throw std::runtime_error("failed to query output buffer");

    auto output_size = buffer.m.planes[0].length;
    auto output_offset = buffer.m.planes[0].m.mem_offset;
    auto output_mem = mmap(
            nullptr, output_size, PROT_READ | PROT_WRITE, MAP_SHARED, decoder_fd, output_offset
    );
    if (output_mem == MAP_FAILED)
        throw std::runtime_error("failed to mmap output buffer");


    std::cout << "V4l2 Decoder MMAPed output buffer with size like so: " <<
        output_size << ", " << output_offset << std::endl;

    std::cout << "MUST BE BIGGER THEN: " << input_size << std::endl;

    memcpy((void *)output_mem, (void *) in_buf.data(), input_size);

    /*
     * SETUP CAPTURE BUFFERS
     */

    buffer = {};
    memset(planes, 0, sizeof(planes));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = 0;
    buffer.length = 1;
    buffer.m.planes = planes;
    if (xioctl(decoder_fd, VIDIOC_QUERYBUF, &buffer) < 0)
        throw std::runtime_error("failed to query capture buffer");

    auto capture_size = buffer.m.planes[0].length;
    auto capture_offset = buffer.m.planes[0].m.mem_offset;
    auto capture_mem = mmap(
            nullptr, buffer.m.planes[0].length, PROT_READ | PROT_WRITE, MAP_SHARED, decoder_fd,
            buffer.m.planes[0].m.mem_offset
    );
    if (capture_mem == MAP_FAILED)
        throw std::runtime_error("failed to mmap capture buffer");

    std::cout << "V4l2 Decoder MMAPed capture buffer with size like so: " <<
              buffer.m.planes[0].length << ", " << buffer.m.planes[0].m.mem_offset << std::endl;

    /*
     * Queue the capture buffer
     */

    buffer = {};
    memset(planes, 0, sizeof(planes));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = 0;
    buffer.length = 1;
    buffer.m.planes = planes;
    buffer.m.planes[0].length = capture_size;
    buffer.m.planes[0].m.mem_offset = capture_offset;

    if (xioctl(decoder_fd, VIDIOC_QBUF, &buffer) < 0)
        throw std::runtime_error("failed to dequeue capture buffer");

    std::cout << "V4l2 Decoder queued capture buffer" << std::endl;

    /*
     * START DECODER
     */

    int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (xioctl(decoder_fd, VIDIOC_STREAMON, &type) < 0)
        throw std::runtime_error("failed to start output");

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (xioctl(decoder_fd, VIDIOC_STREAMON, &type) < 0)
        throw std::runtime_error("failed to start capture");

    std::cout << "v4l2 decoder started!" << std::endl;

    /*
     * QUEUE OUTPUT BUFFER
     */

    buffer = {};
    memset(planes, 0, sizeof(planes));
    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buffer.index = 0;
    buffer.field = V4L2_FIELD_NONE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.length = 1;
    buffer.timestamp.tv_sec = 0;
    buffer.timestamp.tv_usec = 33000;
    buffer.m.planes = planes;
    buffer.m.planes[0].length = output_size;
    buffer.m.planes[0].bytesused = input_size;
    buffer.m.planes[0].m.mem_offset = output_offset;
    if (xioctl(decoder_fd, VIDIOC_QBUF, &buffer) < 0)
        throw std::runtime_error("failed to queue output buffer");

    std::cout << "v4l2 decoder queued output buffer" << std::endl;

    /*
    if (xioctl(decoder_fd, VIDIOC_DQBUF, &buffer) < 0)
        throw std::runtime_error("failed to queue output buffer");

    buffer.timestamp.tv_sec = 0;
    buffer.timestamp.tv_usec = 33000;

    if (xioctl(decoder_fd, VIDIOC_QBUF, &buffer) < 0)
        throw std::runtime_error("failed to queue output buffer");

    std::cout << "did multiple cycles" << std::endl;
     */

    /*
     * Dequeue the capture buffer
     */

    buffer = {};
    memset(planes, 0, sizeof(planes));
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buffer.memory = V4L2_MEMORY_MMAP;
    buffer.index = 0;
    buffer.length = 1;
    buffer.m.planes = planes;
    buffer.m.planes[0].length = capture_size;
    buffer.m.planes[0].m.mem_offset = capture_offset;

    if (xioctl(decoder_fd, VIDIOC_DQBUF, &buffer) < 0)
        throw std::runtime_error("failed to dequeue capture buffer");

    std::cout << "v4l2 decoder dequeued capture buffer" << std::endl;

    memcpy((void *)out_buf.data(), (void *) capture_mem, capture_size);

    std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
    test_file_out.write(out_buf.data(), capture_size);
    test_file_out.flush();
    test_file_out.close();

    /*
     * STOP DECODER
     */

    type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (xioctl(decoder_fd, VIDIOC_STREAMOFF, &type) < 0)
        throw std::runtime_error("failed to start output");

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (xioctl(decoder_fd, VIDIOC_STREAMOFF, &type) < 0)
        throw std::runtime_error("failed to start capture");

    std::cout << "V4l2 Decoder stopped" << std::endl;

    /*
     * CLEANUP
     */

    munmap(capture_mem, capture_size);
    munmap(output_mem, output_size);

    reqbufs = {};
    reqbufs.count = 0;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    if (xioctl(decoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
        std::cout << "Failed to free output buffers" << std::endl;
    }

    reqbufs = {};
    reqbufs.count = 0;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    if (xioctl(decoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
        std::cout << "Failed to free capture buffers" << std::endl;
    }

    std::cout << "v4l2 Decoder cleaned up buffers" << std::endl;


    close(decoder_fd);
    std::cout << "V4l2 Decoder Closed" << std::endl;
    return 0;
}
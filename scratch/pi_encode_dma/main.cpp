

#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

#include <linux/videodev2.h>

#include <filesystem>
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

int alloc_dma_buf(size_t size, int* fd, void** addr) {
    int ret;
    struct dma_heap_allocation_data alloc_data = {0};
    alloc_data.len = size;
    alloc_data.fd_flags = O_CLOEXEC | O_RDWR;

    int heap_fd = open("/dev/dma_heap/system", O_RDWR | O_CLOEXEC, 0);
    if (heap_fd < 0) {
        perror("Error opening file");
        return -1;
    }

    if (xioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc_data) < 0) {
        close(heap_fd);
        perror("ioctl DMA_HEAP_IOCTL_ALLOC failed");
        return -1;
    }

    *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, alloc_data.fd, 0);
    if (*addr == MAP_FAILED) {
        perror("mmap failed");
        close(alloc_data.fd);
        close(heap_fd);
        return -1;
    }

    *fd = alloc_data.fd;
    close(heap_fd);
    return 0;
}

void free_dma_buf(int fd, void* addr, size_t size) {
    munmap(addr, size);
    close(fd);
}


int main(int argc, char *argv[]) {
    const char device_name[] = "/dev/video11";
    const int width = 1536;
    const int height = 864;
    const int stride = 1536;
    const int max_size = width * height * 3 / 2;

    std::filesystem::path this_dir = TMP_DIR;

    auto in_frame = this_dir;
    in_frame /= "in.yuv";

    auto out_frame = this_dir;
    out_frame /= "out.jpeg";

    if(std::filesystem::remove(out_frame)) {
        std::cout << "Removed output file" << std::endl;
    } else {
        std::cout << "No output file to remove" << std::endl;
    }

    std::ifstream test_in_file(in_frame, std::ios::in | std::ios::binary);
    std::array<char, 1990656> in_buf = {};
    test_in_file.read(in_buf.data(), max_size);

    std::chrono::time_point<std::chrono::high_resolution_clock> in_time, out_time;


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
    std::cout << "V4l2 Encoder opened fd: " << encoder_fd << std::endl;

    /*
     *  check formats
     */

    v4l2_fmtdesc fmtdesc{0};
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    for (int i = 0;; ++i) {
        fmtdesc.index = i;
        if (xioctl(encoder_fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1) {
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
        if (xioctl(encoder_fd, VIDIOC_ENUM_FMT, &fmtdesc) == -1) {
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
     * CREATE DMA BUFFER
     */
    void *dma_mem = nullptr;
    int dma_fd = -1;
    if (alloc_dma_buf(max_size, &dma_fd, &dma_mem) < 0) {
        throw std::runtime_error("unable to allocate dma buffer");
    }

    memcpy((void *)dma_mem, (void *) in_buf.data(), max_size);

    /*
     *  SET FORMATS
     */


    v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt.fmt.pix_mp.width = width;
    fmt.fmt.pix_mp.height = height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420;
    fmt.fmt.pix_mp.plane_fmt[0].bytesperline = stride;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;
    fmt.fmt.pix_mp.num_planes = 1;

    if (xioctl(encoder_fd, VIDIOC_S_FMT, &fmt))
        throw std::runtime_error("failed to set output caps");


    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.width = width;
    fmt.fmt.pix_mp.height = height;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
    fmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_REC709;
    fmt.fmt.pix_mp.num_planes = 1;
    fmt.fmt.pix_mp.plane_fmt[0].bytesperline = 0;
    fmt.fmt.pix_mp.plane_fmt[0].sizeimage = 512 << 10;

    if (xioctl(encoder_fd, VIDIOC_S_FMT, &fmt))
        throw std::runtime_error("failed to set output caps");

    std::cout << "V4l2 Encoder setup caps" << std::endl;

    /*
     * REQUEST BUFFERS
     */

    v4l2_requestbuffers reqbufs = {};
    reqbufs.count = 1;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    // reqbufs.memory = V4L2_MEMORY_MMAP;
    reqbufs.memory = V4L2_MEMORY_DMABUF;
    if (xioctl(encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
        std::cout << errno << std::endl;
        throw std::runtime_error("request for output buffers failed");
    }
    std::cout << "V4L2 Encoder got " << reqbufs.count << " output buffers" << std::endl;

    reqbufs = {};
    reqbufs.count = 1;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    if (xioctl(encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
        std::cout << errno << std::endl;
        throw std::runtime_error("request for capture buffers failed");
    }
    std::cout << "V4L2 Encoder got " << reqbufs.count << " capture buffers" << std::endl;


    /*
     * SETUP OUTPUT BUFFERS
     */

    v4l2_plane planes[VIDEO_MAX_PLANES];
    v4l2_buffer buffer = {};


    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buffer.memory = V4L2_MEMORY_DMABUF;
    buffer.index = 0;
    buffer.length = 1;
    buffer.m.planes = planes;

    if (xioctl(encoder_fd, VIDIOC_QUERYBUF, &buffer) < 0)
        throw std::runtime_error("failed to query output buffer");

    /*





    auto output_size = buffer.m.planes[0].length;
    auto output_offset = buffer.m.planes[0].m.mem_offset;
    auto output_mem = mmap(
            nullptr, output_size, PROT_READ | PROT_WRITE, MAP_SHARED, encoder_fd, output_offset
    );
    if (output_mem == MAP_FAILED)
        throw std::runtime_error("failed to mmap output buffer");


    std::cout << "V4l2 Encoder MMAPed output buffer with size like so: " <<
        output_size << ", " << output_offset << std::endl;

    memcpy((void *)output_mem, (void *) in_buf.data(), max_size);

     */

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
    if (xioctl(encoder_fd, VIDIOC_QUERYBUF, &buffer) < 0)
        throw std::runtime_error("failed to query capture buffer");

    auto capture_size = buffer.m.planes[0].length;
    auto capture_offset = buffer.m.planes[0].m.mem_offset;
    auto capture_mem = mmap(
            nullptr, capture_size, PROT_READ | PROT_WRITE, MAP_SHARED, encoder_fd, capture_offset
    );
    if (capture_mem == MAP_FAILED)
        throw std::runtime_error("failed to mmap capture buffer");

    std::cout << "V4l2 Encoder MMAPed capture buffer with size like so: " <<
        capture_size << ", " << capture_offset << std::endl;

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

    if (xioctl(encoder_fd, VIDIOC_QBUF, &buffer) < 0)
        throw std::runtime_error("failed to dequeue capture buffer");

    std::cout << "V4l2 Encoder queued capture buffer" << std::endl;

    /*
     * START DECODER
     */

    int type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (xioctl(encoder_fd, VIDIOC_STREAMON, &type) < 0)
        throw std::runtime_error("failed to start output");

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (xioctl(encoder_fd, VIDIOC_STREAMON, &type) < 0)
        throw std::runtime_error("failed to start capture");

    std::cout << "v4l2 Encoder started!" << std::endl;

    /*
     * QUEUE OUTPUT BUFFER
     */

    buffer = {};
    memset(planes, 0, sizeof(planes));
    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buffer.index = 0;
    buffer.field = V4L2_FIELD_NONE;
    buffer.memory = V4L2_MEMORY_DMABUF;
    buffer.length = 1;
    buffer.timestamp.tv_sec = 0;
    buffer.timestamp.tv_usec = 0;
    buffer.m.planes = planes;
    buffer.m.planes[0].length = max_size;
    buffer.m.planes[0].bytesused = max_size;
    buffer.m.planes[0].m.fd = dma_fd;
    if (xioctl(encoder_fd, VIDIOC_QBUF, &buffer) < 0)
        throw std::runtime_error("failed to queue output buffer");

    std::cout << "v4l2 decoder queued output buffer" << std::endl;


    if (xioctl(encoder_fd, VIDIOC_DQBUF, &buffer) < 0)
        throw std::runtime_error("failed to queue output buffer");

    buffer.timestamp.tv_sec = 0;
    buffer.timestamp.tv_usec = 33000;


    if (xioctl(encoder_fd, VIDIOC_QBUF, &buffer) < 0)
        throw std::runtime_error("failed to queue output buffer");

    in_time = Clock::now();
    std::cout << "did multiple cycles" << std::endl;

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

    if (xioctl(encoder_fd, VIDIOC_DQBUF, &buffer) < 0)
        throw std::runtime_error("failed to dequeue capture buffer");

    out_time = Clock::now();

    auto out_size = buffer.m.planes[0].bytesused;
    std::cout << "v4l2 decoder dequeued capture buffer with size: " << out_size << std::endl;

    std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
    test_file_out.write((char *) capture_mem, out_size);
    test_file_out.flush();
    test_file_out.close();

    /*
     * STOP ENCODER
     */

    type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (xioctl(encoder_fd, VIDIOC_STREAMOFF, &type) < 0)
        throw std::runtime_error("failed to start output");

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (xioctl(encoder_fd, VIDIOC_STREAMOFF, &type) < 0)
        throw std::runtime_error("failed to start capture");

    std::cout << "V4l2 Decoder stopped" << std::endl;

    /*
     * CLEANUP
     */

    munmap(capture_mem, capture_size);
    free_dma_buf(dma_fd, dma_mem, max_size);

    reqbufs = {};
    reqbufs.count = 0;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    if (xioctl(encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
        std::cout << "Failed to free output buffers" << std::endl;
    }

    reqbufs = {};
    reqbufs.count = 0;
    reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    reqbufs.memory = V4L2_MEMORY_MMAP;
    if (xioctl(encoder_fd, VIDIOC_REQBUFS, &reqbufs) < 0) {
        std::cout << "Failed to free capture buffers" << std::endl;
    }

    std::cout << "v4l2 Encoder cleaned up buffers" << std::endl;


    close(encoder_fd);
    std::cout << "V4l2 Encoder Closed" << std::endl;

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(out_time - in_time);
    std::cout << "Time to encode: " << d1.count() << std::endl;

    return 0;
}
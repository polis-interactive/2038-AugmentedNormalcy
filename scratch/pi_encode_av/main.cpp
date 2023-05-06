

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
#include <thread>

#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;


extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/hwcontext.h"
#include "libavutil/hwcontext_drm.h"
#include "libavutil/imgutils.h"
#include "libavutil/timestamp.h"
#include "libavutil/version.h"
#include "libswresample/swresample.h"
}

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

void print_buffer_info(const v4l2_buffer &buffer, const v4l2_plane *planes) {
    std::cout << "Buffer information:\n";
    std::cout << "  Type: " << buffer.type << "\n";
    std::cout << "  Memory: " << buffer.memory << "\n";
    std::cout << "  Index: " << buffer.index << "\n";
    std::cout << "  Length: " << buffer.length << "\n";
    std::cout << "  flags: " << buffer.flags << "\n";
    std::cout << "  field: " << buffer.field << "\n";

    for (unsigned int i = 0; i < buffer.length; ++i) {
        std::cout << "  Plane " << i << ":\n";
        std::cout << "    Bytes used: " << planes[i].bytesused << "\n";
        std::cout << "    Length: " << planes[i].length << "\n";
        std::cout << "    Data offset: " << planes[i].data_offset << "\n";
        std::cout << "    DMABUF file descriptor: " << planes[i].m.fd << "\n";
    }
}


int main(int argc, char *argv[]) {
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


    /*
     * CREATE DMA BUFFER
     */
    void *dma_mem = nullptr;
    int dma_fd = -1;
    if (alloc_dma_buf(max_size, &dma_fd, &dma_mem) < 0) {
        throw std::runtime_error("unable to allocate dma buffer");
    }

    memcpy((void *)dma_mem, (void *) in_buf.data(), max_size);



    const AVCodec *codec = avcodec_find_encoder_by_name("mjpeg_v4l2m2m");
    if (!codec)
        throw std::runtime_error("libav: cannot find video encoder mjpeg_v4l2m2m");

    /*
     * CLEANUP
     */

    free_dma_buf(dma_fd, dma_mem, max_size);

    return 0;
}
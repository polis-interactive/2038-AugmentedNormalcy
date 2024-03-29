

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

#include <vector>

#include <jpeglib.h>

#if JPEG_LIB_VERSION_MAJOR > 9 || (JPEG_LIB_VERSION_MAJOR == 9 && JPEG_LIB_VERSION_MINOR >= 4)
typedef size_t jpeg_mem_len_t;
#else
typedef unsigned long jpeg_mem_len_t;
#endif


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
    const int width = 1536;
    const int height = 864;
    const int stride = 1536;
    const int max_size = width * height * 3 / 2;

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

    /*
     * CREATE DMA BUFFER
     */


    void *dma_mem = nullptr;
    int dma_fd = -1;
    if (alloc_dma_buf(max_size, &dma_fd, &dma_mem) < 0) {
        throw std::runtime_error("unable to allocate dma buffer");
    }


    /*
     * setup encoder
     */

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    std::chrono::duration<double> encode_time(0);
    uint32_t frames = 0;

    for (int inc = 0; inc < 500; inc++) {

        auto start_time = std::chrono::high_resolution_clock::now();

        jpeg_mem_src(&cinfo, (unsigned char*)in_buf.data(), input_size);
        jpeg_read_header(&cinfo, TRUE);
        cinfo.out_color_space = JCS_YCbCr;
        cinfo.raw_data_out = TRUE;
        jpeg_start_decompress(&cinfo);

        int stride2 = stride / 2;
        uint8_t *Y = (uint8_t *) dma_mem;
        uint8_t *U = Y + stride * height;
        uint8_t *V = U + stride2 * (height / 2);

        JSAMPROW y_rows[16];
        JSAMPROW u_rows[8];
        JSAMPROW v_rows[8];
        JSAMPARRAY data[] = { y_rows, u_rows, v_rows };

        while (cinfo.output_scanline < height) {
            for (int i = 0; i < 16 && cinfo.output_scanline < height; ++i, Y += stride) {
                y_rows[i] = Y;
            }

            for (int i = 0; i < 8 && cinfo.output_scanline < height; ++i, U += stride2, V += stride2) {
                u_rows[i] = U;
                v_rows[i] = V;
            }

            jpeg_read_raw_data(&cinfo, data, 16);
        }

        jpeg_finish_decompress(&cinfo);

        encode_time += (std::chrono::high_resolution_clock::now() - start_time);

    }

    jpeg_destroy_decompress(&cinfo);

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(encode_time);
    std::cout << "Time to decode total: " << d1.count()  << ", average: " << d1.count() / 500 << std::endl;

    std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
    test_file_out.write((char *)dma_mem, max_size);
    test_file_out.flush();
    test_file_out.close();

    /*
     * CLEANUP
     */

    free_dma_buf(dma_fd, dma_mem, max_size);

    return 0;
}
//
// Created by brucegoose on 3/30/23.
//

#include <utility>
#include <filesystem>
#include <fstream>
#include <sys/mman.h>

#include <vector>
#include <thread>
#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;

#include "NvUtils.h"
#include "NvJpegEncoder.h"
#include "NvBufSurface.h"
#include "libjpeg-8b/jpeglib.h"

void user_buffer() {
    std::filesystem::path this_dir = TMP_DIR;

    auto in_frame = this_dir;
    in_frame /= "in.yuv";

    auto out_frame = this_dir;
    out_frame /= "out_user.jpeg";

    NvBuffer buffer(V4L2_PIX_FMT_YUV420M, 1536, 864, 0);
    buffer.allocateMemory();

    std::ifstream test_file_in(in_frame, std::ios::out | std::ios::binary);
    auto ret = read_video_frame(&test_file_in, buffer);

    auto jpegenc = NvJPEGEncoder::createJPEGEncoder("jpenenc");
    unsigned long out_buf_size = 1536 * 864 * 3 / 2;
    auto *out_buf = new unsigned char[out_buf_size];

    ret = jpegenc->encodeFromBuffer(buffer, JCS_YCbCr, &out_buf, out_buf_size, 75);

    std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);

    test_file_out.write((char *) out_buf, out_buf_size);

    delete[] out_buf;
    delete jpegenc;
}

void dma_buffer() {

    std::filesystem::path this_dir = TMP_DIR;

    auto in_frame = this_dir;
    in_frame /= "in.yuv";

    auto out_frame = this_dir;
    out_frame /= "out_dma.jpeg";

    NvBufSurf::NvCommonAllocateParams params;
    /* Create PitchLinear output buffer for transform. */
    params.memType = NVBUF_MEM_SURFACE_ARRAY;
    params.width = 1536;
    params.height = 864;
    params.layout = NVBUF_LAYOUT_PITCH;
    params.colorFormat = NVBUF_COLOR_FORMAT_YUV420;

    params.memtag = NvBufSurfaceTag_CAMERA;
    int src_dma_fd = -1;
    auto ret = NvBufSurf::NvAllocate(&params, 1, &src_dma_fd);

    std::ifstream test_file_in(in_frame, std::ios::out | std::ios::binary);
    read_dmabuf(src_dma_fd, 0, &test_file_in);
    read_dmabuf(src_dma_fd, 1, &test_file_in);
    read_dmabuf(src_dma_fd, 2, &test_file_in);

    auto jpegenc = NvJPEGEncoder::createJPEGEncoder("jpenenc");
    unsigned long out_buf_size = 1536 * 864 * 3 / 2;
    auto *out_buf = new unsigned char[out_buf_size];

    ret = jpegenc->encodeFromFd(src_dma_fd, JCS_YCbCr, &out_buf, out_buf_size, 75);

    std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);

    test_file_out.write((char *) out_buf, out_buf_size);

    delete[] out_buf;
    delete jpegenc;
    NvBufSurf::NvDestroy(src_dma_fd);
}

class MmapDmaBuffer {
public:
    MmapDmaBuffer() {

        NvBufSurf::NvCommonAllocateParams params;
        /* Create PitchLinear output buffer for transform. */
        params.memType = NVBUF_MEM_SURFACE_ARRAY;
        params.width = 1536;
        params.height = 864;
        params.layout = NVBUF_LAYOUT_PITCH;
        params.colorFormat = NVBUF_COLOR_FORMAT_YUV420;

        params.memtag = NvBufSurfaceTag_CAMERA;

        auto ret = NvBufSurf::NvAllocate(&params, 1, &fd);

        // just going to mmap it myself
        _memory = mmap(NULL, 1327104, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        auto m1 = mmap(
            (uint8_t *) _memory + 1327104, 331776, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 1441792
        );
        auto m2 = mmap(
            (uint8_t *) _memory + 1327104 + 331776, 331776, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 1835008
        );

    }
    char * get_memory() {
        return (char *) _memory;
    }
    [[nodiscard]] int get_fd() const {
        return fd;
    }
    std::size_t get_size() {
        return 1990656;
    }
    ~MmapDmaBuffer() {
        std::cout << "removing!" << std::endl;
        if (_memory != nullptr) {
            munmap(_memory, 1990656);
        }

        if (fd != -1) {
            NvBufSurf::NvDestroy(fd);
            fd = -1;
        }
    }
private:
    int fd = -1;
    void * _memory = nullptr;
};

void mmap_buffer() {
    MmapDmaBuffer buffer;

    std::filesystem::path this_dir = TMP_DIR;

    auto in_frame = this_dir;
    in_frame /= "in.yuv";

    auto out_frame = this_dir;
    out_frame /= "out_mmap.jpeg";

    std::ifstream test_in_file(in_frame, std::ios::in | std::ios::binary);
    std::cout << "reported size: " << buffer.get_size() << std::endl;
    std::cout << "I think size: " << 1990656 << std::endl;
    test_in_file.read(buffer.get_memory(), 1990656);

    auto jpegenc = NvJPEGEncoder::createJPEGEncoder("jpenenc");
    unsigned long out_buf_size = 1536 * 864 * 3 / 2;
    auto *out_buf = new unsigned char[out_buf_size];

    auto ret = jpegenc->encodeFromFd(buffer.get_fd(), JCS_YCbCr, &out_buf, out_buf_size, 75);

    std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);

    test_file_out.write((char *) out_buf, out_buf_size);

    delete[] out_buf;
    delete jpegenc;

}

void stress_test_mmap() {
    std::vector<MmapDmaBuffer*> vec;
    for (int i = 0; i < 8 * 6 * 2; i++) {
        auto buf = new MmapDmaBuffer();
        vec.push_back(buf);
    }

    std::cout << "any nullptrs in the chat?" << std::endl;
    for (int i = 0; i < 8 * 6 * 2; i++) {
        auto buf = vec.back();
        delete buf;
        vec.pop_back();
    }
}

void run_thread_test(const int thread_number) {
    try {
        std::cout << thread_number << " Starting" << std::endl;

        std::filesystem::path this_dir = TMP_DIR;

        auto in_frame = this_dir;
        in_frame /= "in.yuv";

        MmapDmaBuffer buffer;

        std::cout << thread_number << " Created buffer" << std::endl;

        std::ifstream test_in_file(in_frame, std::ios::in | std::ios::binary);

        std::array<unsigned char, 1990656> in_buf = {};
        test_in_file.read(in_buf.data(), 1990656);

        auto jpegenc = NvJPEGEncoder::createJPEGEncoder("jpenenc");
        unsigned long out_buf_size = 1536 * 864 * 3 / 2;
        std::array<unsigned char, 1990656> out_buf = {};

        std::cout << thread_number << " Created encoder" << std::endl;

        bool found_diff = false;

        auto buf_ptr = out_buf.data();


        for (int i = 0; i < 100; i++) {
            memcpy(buffer.get_memory(), (void *) in_buf.data(), 1990656);
            jpegenc->encodeFromFd(buffer.get_fd(), JCS_YCbCr, &buf_ptr, out_buf_size, 75);
        }
        if (!found_diff) {
            std::cout << thread_number << " no difference here :D" << std::endl;
        } else {
            std::cout << thread_number << " there were difss abound" << std::endl;
        }

        std::cout <<  thread_number << " removing encoder" << std::endl;
        delete jpegenc;
    } catch(...) {
        std::cout << thread_number << " failed?" << std::endl;
    }

    std::cout <<  thread_number << " exiting" << std::endl;
}

void thread_test() {
    std::vector<std::thread> threads;
    std::cout << "Starting threads" << std::endl;
    std::chrono::time_point< std::chrono::high_resolution_clock> t1, t2;
    for (int i = 0; i < 9; i++) {
        threads.emplace_back(std::thread(run_thread_test, i));
    }
    t1 = Clock::now();

    std::cout << "Waiting for threads to finish" << std::endl;
    for (auto &th: threads) {
        th.join();
    }

    t2 = Clock::now();


    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    std::cout << "roughly took " << d1.count() << " milliseconds to run" << std::endl;
}

int main(int argc, char *argv[]) {
    thread_test();
}
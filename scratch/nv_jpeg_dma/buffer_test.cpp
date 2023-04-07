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
        if (ret < 0) {
            std::cout << "Error allocating buffer: " << ret << std::endl;
        }


        ret = NvBufSurfaceFromFd(fd, (void**)(&_nvbuf_surf));
        if (ret != 0)
        {
            std::cout << "failed to get surface from fd" << std::endl;
        }

        _size = _nvbuf_surf->surfaceList->planeParams.height[0] * _nvbuf_surf->surfaceList->planeParams.pitch[0];
        _size_1 = _nvbuf_surf->surfaceList->planeParams.height[1] * _nvbuf_surf->surfaceList->planeParams.pitch[1];
        _size_2 = _nvbuf_surf->surfaceList->planeParams.height[2] * _nvbuf_surf->surfaceList->planeParams.pitch[2];

        // just going to mmap it myself
        _memory = mmap(
                NULL,
                _size,
                PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                _nvbuf_surf->surfaceList->planeParams.offset[0]
        );
        if (_memory == MAP_FAILED) {
            std::cout << "FAILED TO MMAP AT ADDRESS" << std::endl;
        }
        _memory_1 = mmap(
            (uint8_t *) _memory + _size,
            _size_1,
            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd,
            _nvbuf_surf->surfaceList->planeParams.offset[1]
        );
        if (_memory_1 == MAP_FAILED) {
            std::cout << "FAILED TO MMAP AT ADDRESS" << std::endl;
        }
        _memory_2 = mmap(
            (uint8_t *) _memory_1 + _size_1,
            _size_2,
            PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd,
            _nvbuf_surf->surfaceList->planeParams.offset[2]
        );
        if (_memory_2 == MAP_FAILED) {
            std::cout << "FAILED TO MMAP AT ADDRESS" << std::endl;
        }

    }
    char * get_memory() {
        return (char *) _memory;
    }
    [[nodiscard]] int get_fd() const {
        return fd;
    }
    std::size_t get_size() {
        return _size + _size_1 + _size_2;
    }
    ~MmapDmaBuffer() {
        std::cout << "removing!" << std::endl;
        if (_memory != nullptr) {
            munmap(_memory, _size);
        }
        if (_memory_1 != nullptr) {
            munmap(_memory, _size_1);

        }
        if (_memory_2 != nullptr) {
            munmap(_memory, _size_2);

        }
        if (fd != -1) {
            NvBufSurf::NvDestroy(fd);
            fd = -1;
        }
    }
private:
    int fd = -1;
    void * _memory = nullptr;
    void * _memory_1 = nullptr;
    void * _memory_2 = nullptr;
    NvBufSurface *_nvbuf_surf = 0;
    std::size_t _size = 0;
    std::size_t _size_1 = 0;
    std::size_t _size_2 = 0;
};

void mmap_buffer() {
    MmapDmaBuffer buffer;

    std::filesystem::path this_dir = TMP_DIR;

    auto in_frame = this_dir;
    in_frame /= "in.yuv";

    auto out_frame = this_dir;
    out_frame /= "out_mmap.jpeg";

    std::ifstream test_in_file(in_frame, std::ios::in | std::ios::binary);

    std::cout << buffer.get_size() << ", " << 1990656 << "?" << std::endl;


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

        std::array<char, 1990656> in_buf = {};
        test_in_file.read(in_buf.data(), 1990656);

        auto jpegenc = NvJPEGEncoder::createJPEGEncoder("jpenenc");
        unsigned long out_buf_size = 1536 * 864 * 3 / 2;
        std::array<unsigned char, 1990656> out_buf = {};

        std::cout << thread_number << " Created encoder" << std::endl;

        bool found_diff = false;

        auto buf_ptr = out_buf.data();

        std::cout << (void *) out_buf.data() << ", " << (void *) buffer.get_memory() << std::endl;


        for (int i = 0; i < 100; i++) {
            // buffer.sync_for_cpu();
            memcpy(buffer.get_memory(), (void *) in_buf.data(), buffer.get_size());
            // buffer.sync_for_gpu();
            auto buf_ptr = out_buf.data();
            auto ret = jpegenc->encodeFromFd(buffer.get_fd(), JCS_YCbCr, &buf_ptr, out_buf_size, 75);
            if (ret < 0) {
                std::cout << thread_number << " Error while encoding from fd" << std::endl;
                break;
            }
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

    t1 = Clock::now();
    for (int i = 0; i < 9; i++) {
        threads.emplace_back(std::thread(run_thread_test, i));
        std::this_thread::sleep_for(0.5s);
    }

    std::cout << "Waiting for threads to finish" << std::endl;
    for (auto &th: threads) {
        th.join();
    }

    t2 = Clock::now();


    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    std::cout << "roughly took " << d1.count() << " milliseconds to run" << std::endl;
}

class NvBufferCapn {
public:
    NvBufferCapn() {

        NvBufSurf::NvCommonAllocateParams params;
        /* Create PitchLinear output buffer for transform. */
        params.memType = NVBUF_MEM_SURFACE_ARRAY;
        params.width = 1536;
        params.height = 864;
        params.layout = NVBUF_LAYOUT_PITCH;
        params.colorFormat = NVBUF_COLOR_FORMAT_YUV420;

        params.memtag = NvBufSurfaceTag_CAMERA;

        auto ret = NvBufSurf::NvAllocate(&params, 1, &fd);
        if (ret < 0) {
            std::cout << "Error allocating buffer: " << ret << std::endl;
        }


        ret = NvBufSurfaceFromFd(fd, (void**)(&_nvbuf_surf));
        if (ret != 0)
        {
            std::cout << "failed to get surface from fd" << std::endl;
        }

        NvBufSurfaceMap(_nvbuf_surf, 0, 0, NVBUF_MAP_READ_WRITE);
        NvBufSurfaceMap(_nvbuf_surf, 0, 1, NVBUF_MAP_READ_WRITE);
        NvBufSurfaceMap(_nvbuf_surf, 0, 2, NVBUF_MAP_READ_WRITE);

    }
    char * get_plane_0() {
        return (char *)  _nvbuf_surf->surfaceList->mappedAddr.addr[0];
    }
    std::size_t get_size_0() {
        return _nvbuf_surf->surfaceList->planeParams.height[0] * _nvbuf_surf->surfaceList->planeParams.pitch[0];
    }
    char * get_plane_1() {
        return (char *)  _nvbuf_surf->surfaceList->mappedAddr.addr[1];
    }
    std::size_t get_size_1() {
        return _nvbuf_surf->surfaceList->planeParams.height[1] * _nvbuf_surf->surfaceList->planeParams.pitch[1];
    }
    char * get_plane_2() {
        return (char *)  _nvbuf_surf->surfaceList->mappedAddr.addr[2];
    }
    std::size_t get_size_2() {
        return _nvbuf_surf->surfaceList->planeParams.height[2] * _nvbuf_surf->surfaceList->planeParams.pitch[2];
    }
    void sync_for_cpu() {
        NvBufSurfaceSyncForCpu(_nvbuf_surf, 0, 0);
        NvBufSurfaceSyncForCpu(_nvbuf_surf, 0, 1);
        NvBufSurfaceSyncForCpu(_nvbuf_surf, 0, 2);
    }
    void sync_for_gpu() {
        NvBufSurfaceSyncForDevice(_nvbuf_surf, 0, 0);
        NvBufSurfaceSyncForDevice(_nvbuf_surf, 0, 1);
        NvBufSurfaceSyncForDevice(_nvbuf_surf, 0, 2);
    }
    [[nodiscard]] int get_fd() const {
        return fd;
    }
    ~NvBufferCapn() {
        NvBufSurfaceUnMap(_nvbuf_surf, 0, 0);
        NvBufSurfaceUnMap(_nvbuf_surf, 0, 1);
        NvBufSurfaceUnMap(_nvbuf_surf, 0, 2);
        if (fd != -1) {
            NvBufSurf::NvDestroy(fd);
            fd = -1;
        }
    }
private:
    int fd = -1;
    void * _memory = nullptr;
    void * _memory_1 = nullptr;
    void * _memory_2 = nullptr;
    NvBufSurface *_nvbuf_surf = 0;
    std::size_t _size = 0;
    std::size_t _size_1 = 0;
    std::size_t _size_2 = 0;
};

void nv_buffer_test() {
    auto buffer = new NvBufferCapn();
    auto buffer_1 = new NvBufferCapn();

    auto jpegenc = NvJPEGEncoder::createJPEGEncoder("jpenenc");
    unsigned long out_buf_size = 1536 * 864 * 3 / 2;
    auto *out_buf = new unsigned char[out_buf_size];

    std::filesystem::path this_dir = TMP_DIR;

    auto in_frame = this_dir;
    in_frame /= "in.yuv";

    auto out_frame = this_dir;
    out_frame /= "out_mmap.jpeg";

    std::ifstream test_in_file(in_frame, std::ios::in | std::ios::binary);

    // buffer->sync_for_cpu();
    test_in_file.read(buffer->get_plane_0(), buffer->get_size_0());
    test_in_file.read(buffer->get_plane_1(), buffer->get_size_1());
    test_in_file.read(buffer->get_plane_2(), buffer->get_size_2());
    // buffer->sync_for_gpu();

    auto ret = jpegenc->encodeFromFd(buffer->get_fd(), JCS_YCbCr, &out_buf, out_buf_size, 75);

    std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);

    test_file_out.write((char *) out_buf, out_buf_size);

    delete[] out_buf;
    delete buffer;
    delete buffer_1;
    delete jpegenc;
}

int main(int argc, char *argv[]) {
    nv_buffer_test();
}
//
// Created by brucegoose on 5/5/23.
//

#include "fake_camera.hpp"

#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>
#include <linux/videodev2.h>
#include <filesystem>
#include <cstring>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>

int fxioctl(int fd, unsigned long ctl, void *arg) {
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

    if (fxioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc_data) < 0) {
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

FakeCamera::FakeCamera(const int buffer_count) {
    const int buffer_size = 1536 * 864 * 3 / 2;
    for (size_t i = 0; i < buffer_count; ++i) {
        void *capture_mem = nullptr;
        int fd = -1;
        if (alloc_dma_buf(buffer_size, &fd, &capture_mem) < 0) {
            throw std::runtime_error("unable to allocate dma buffer");
        }
        auto camera_buffer = new CameraBuffer(nullptr, capture_mem, fd, buffer_size, 0);
        _camera_buffers.push_back(camera_buffer);
    }
}

std::shared_ptr<CameraBuffer> FakeCamera::GetBuffer() {
    CameraBuffer *buffer = nullptr;
    {
        std::unique_lock<std::mutex> lock(_camera_mutex);
        if (!_camera_buffers.empty()) {
            buffer = _camera_buffers.front();
            _camera_buffers.pop_front();
        }
    }
    if (buffer == nullptr) {
        return nullptr;
    }
    auto out_buffer = std::shared_ptr<CameraBuffer>(buffer, [this](CameraBuffer *b) {
        std::unique_lock<std::mutex> lock(_camera_mutex);
        _camera_buffers.push_back(b);
    });
    return out_buffer;
}

FakeCamera::~FakeCamera() {
    for (auto &c_buffer: _camera_buffers) {
        free_dma_buf(c_buffer->GetFd(), c_buffer->GetMemory(), c_buffer->GetSize());
    }
}
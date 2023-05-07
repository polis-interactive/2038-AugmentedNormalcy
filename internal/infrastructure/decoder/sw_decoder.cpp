//
// Created by brucegoose on 4/12/23.
//

#include "sw_decoder.hpp"

#include <fcntl.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/dma-heap.h>

#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;

namespace infrastructure {

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

    SwDecoder::SwDecoder(const DecoderConfig &config, DecoderBufferCallback output_callback):
        _output_callback(std::move(output_callback)),
        _width_height(config.get_decoder_width_height())
    {
        auto downstream_count = config.get_decoder_downstream_buffer_count();
        setupDownstreamBuffers(downstream_count);
    }

    void SwDecoder::Start() {

        if (!_work_stop) {
            return;
        }

        _work_stop = false;

        auto self(shared_from_this());
        _work_thread = std::make_unique<std::thread>([this, s = std::move(self)]() mutable { run(); });

    }

    void SwDecoder::Stop() {
        if (_work_stop) {
            return;
        }

        if (_work_thread) {
            if (_work_thread->joinable()) {
                {
                    std::unique_lock<std::mutex> lock(_work_mutex);
                    _work_stop = true;
                    _work_cv.notify_one();
                }
                _work_thread->join();
            }
            _work_thread.reset();
        }
        // just in case we skipped above
        _work_stop = true;

        while(!_work_queue.empty()) {
            _work_queue.pop();
        }
    }

    void SwDecoder::PostJpegBuffer(std::shared_ptr<SizedBuffer> &&buffer) {
        if (buffer == nullptr || _work_stop) {
            return;
        }
        std::unique_lock<std::mutex> lock(_work_mutex);
        _work_queue.push(std::move(buffer));
        _work_cv.notify_one();
    }

    void SwDecoder::run() {

        struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr jerr;
        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_decompress(&cinfo);

        while(!_work_stop) {
            std::shared_ptr<SizedBuffer> buffer;
            {
                std::unique_lock<std::mutex> lock(_work_mutex);
                _work_cv.wait(lock, [this]() {
                    return !_work_queue.empty() || _work_stop;
                });
                if (_work_stop) {
                    return;
                } else if (_work_queue.empty()) {
                    continue;
                }
                buffer = std::move(_work_queue.front());
                _work_queue.pop();
            }
            decodeBuffer(cinfo, std::move(buffer));
        }

        jpeg_destroy_decompress(&cinfo);
    }

    void SwDecoder::decodeBuffer(struct jpeg_decompress_struct &cinfo, std::shared_ptr<SizedBuffer> &&sz_buffer) {
        DecoderBuffer *buffer = nullptr;
        {
            std::unique_lock<std::mutex> lock(_downstream_buffers_mutex);
            if (!_downstream_buffers.empty()) {
                buffer = _downstream_buffers.front();
                _downstream_buffers.pop();
            }
        }
        if (buffer == nullptr) {
            return;
        }
        jpeg_mem_src(&cinfo, (unsigned char*)sz_buffer->GetMemory(), sz_buffer->GetSize());
        jpeg_read_header(&cinfo, TRUE);
        cinfo.out_color_space = JCS_YCbCr;
        cinfo.raw_data_out = TRUE;
        jpeg_start_decompress(&cinfo);

        int stride2 = _width_height.first / 2;
        uint8_t *Y = (uint8_t *) buffer->GetMemory();
        uint8_t *U = Y + _width_height.first * _width_height.second;
        uint8_t *V = U + stride2 * (_width_height.second / 2);

        JSAMPROW y_rows[16];
        JSAMPROW u_rows[8];
        JSAMPROW v_rows[8];
        JSAMPARRAY data[] = { y_rows, u_rows, v_rows };

        while (cinfo.output_scanline < _width_height.second) {
            for (int i = 0; i < 16 && cinfo.output_scanline < _width_height.second; ++i, Y += _width_height.first) {
                y_rows[i] = Y;
            }
            for (int i = 0; i < 8 && cinfo.output_scanline < _width_height.second; ++i, U += stride2, V += stride2) {
                u_rows[i] = U;
                v_rows[i] = V;
            }
            jpeg_read_raw_data(&cinfo, data, 16);
        }
        jpeg_finish_decompress(&cinfo);

        auto self(shared_from_this());
        auto output_buffer = std::shared_ptr<DecoderBuffer>(
                buffer, [this, s = std::move(self)](DecoderBuffer * e) mutable {
                    queueDownstreamBuffer(e);
                }
        );
        _output_callback(std::move(output_buffer));
    }

    void SwDecoder::queueDownstreamBuffer(DecoderBuffer *d) {
        std::unique_lock<std::mutex> lock(_downstream_buffers_mutex);
        _downstream_buffers.push(d);
    }

    void SwDecoder::setupDownstreamBuffers(unsigned int request_downstream_buffers) {
        const auto max_size = _width_height.first * _width_height.second * 3 / 2;
        for (int i = 0; i < request_downstream_buffers; i++) {
            void *dma_mem = nullptr;
            int dma_fd = -1;
            if (alloc_dma_buf(max_size, &dma_fd, &dma_mem) < 0) {
                throw std::runtime_error("unable to allocate dma buffer");
            }
            auto downstream_buffer = new DecoderBuffer(dma_fd, dma_mem, max_size);
            _downstream_buffers.push(downstream_buffer);
        }
    }

    SwDecoder::~SwDecoder() {
        Stop();
        teardownDownstreamBuffers();
    }

    void SwDecoder::teardownDownstreamBuffers() {
        while(!_downstream_buffers.empty()) {
            auto buffer = _downstream_buffers.front();
            _downstream_buffers.pop();
            free_dma_buf(buffer->GetFd(), buffer->GetMemory(), buffer->GetSize());
            delete buffer;
        }
    }
}
//
// Created by brucegoose on 5/4/23.
//

#ifndef INFRASTRUCTURE_ENCODER_SW_ENCODER_HPP
#define INFRASTRUCTURE_ENCODER_SW_ENCODER_HPP

#include "encoder.hpp"

#include <memory>
#include <queue>
#include <mutex>
#include <map>
#include <atomic>
#include <thread>
#include <iostream>
#include <condition_variable>

#include <jpeglib.h>

#if JPEG_LIB_VERSION_MAJOR > 9 || (JPEG_LIB_VERSION_MAJOR == 9 && JPEG_LIB_VERSION_MINOR >= 4)
typedef size_t jpeg_mem_len_t;
#else
typedef unsigned long jpeg_mem_len_t;
#endif

#include "utils/buffers.hpp"

namespace infrastructure {

    struct EncoderBuffer: public SizedBuffer {
        EncoderBuffer(std::size_t size):
                _max_size(size),
                _size(size)
        {
            _memory = new uint8_t[size];
        }

        [[nodiscard]] void *GetMemory() override {
            return _memory;
        }
        [[nodiscard]] uint8_t **GetMemoryPointer() {
            return &_memory;
        };
        [[nodiscard]] std::size_t GetSize() override {
            return _size;
        }
        [[nodiscard]] std::size_t *GetSizePointer() {
            return &_size;
        }
        void ResetSize() {
            _size = _max_size;
        }
        void SetSize(const std::size_t &size) {
            _size = size;
        }
        ~EncoderBuffer() {
            delete []_memory;
        }
    private:
        uint8_t *_memory = nullptr;
        std::size_t _max_size;
        std::size_t _size;
    };

    class SwEncoder: public std::enable_shared_from_this<SwEncoder>, public Encoder {
    public:
        SwEncoder(const EncoderConfig &config, SizedBufferCallback output_callback);
        void PostCameraBuffer(std::shared_ptr<CameraBuffer> &&buffer) override;
        ~SwEncoder();
    private:
        void StartEncoder() override;
        void StopEncoder() override;

        void setupDownstreamBuffers(unsigned int request_downstream_buffers);
        void run();
        void encodeBuffer(struct jpeg_compress_struct &cinfo, std::shared_ptr<CameraBuffer> &&buffer);
        void queueDownstreamBuffer(EncoderBuffer *e);
        void teardownDownstreamBuffers();

        const std::pair<int, int> _width_height;

        std::mutex _work_mutex;
        std::condition_variable _work_cv;
        std::queue<std::shared_ptr<CameraBuffer>> _work_queue;
        std::unique_ptr<std::thread> _work_thread;
        std::atomic<bool> _work_stop = { true };

        std::mutex _downstream_buffers_mutex;
        std::queue<EncoderBuffer*> _downstream_buffers;
    };

}

#endif //INFRASTRUCTURE_ENCODER_SW_ENCODER_HPP

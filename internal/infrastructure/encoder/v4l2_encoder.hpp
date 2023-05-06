//
// Created by brucegoose on 5/4/23.
//

#ifndef INFRASTRUCTURE_ENCODER_V4L2_ENCODER_HPP
#define INFRASTRUCTURE_ENCODER_V4L2_ENCODER_HPP

#include <memory>
#include <queue>
#include <mutex>
#include <map>
#include <atomic>
#include <thread>
#include <iostream>
#include <condition_variable>

#include "utils/buffers.hpp"

namespace infrastructure {

    struct EncoderBuffer: public SizedBuffer {
        EncoderBuffer(unsigned int buffer_index, void *memory, std::size_t size, std::size_t offset):
                _buffer_index(buffer_index),
                _memory(memory),
                _max_size(size),
                _size(size),
                _offset(offset)
        {}

        [[nodiscard]] void *GetMemory() override {
            return _memory;
        };
        [[nodiscard]] std::size_t GetSize() override {
            return _size;
        };
        void SetSize(const std::size_t &size) {
            _size = size;
        }
        [[nodiscard]] std::size_t GetOffset() const {
            return _offset;
        };
        [[nodiscard]] std::size_t GetIndex() const {
            return _buffer_index;
        };
    private:
        const unsigned int _buffer_index;
        void *_memory;
        std::size_t _max_size;
        std::size_t _size;
        std::size_t _offset;
    };

    struct EncoderConfig {
        [[nodiscard]] virtual unsigned int get_encoder_upstream_buffer_count() const = 0;
        [[nodiscard]] virtual unsigned int get_encoder_downstream_buffer_count() const = 0;
        [[nodiscard]] virtual std::pair<int, int> get_encoder_width_height() const = 0;
    };

    class V4l2Encoder: public std::enable_shared_from_this<V4l2Encoder> {
    public:
        static std::shared_ptr<V4l2Encoder>Create(
                const EncoderConfig &config, SizedBufferCallback output_callback
        ) {
            auto encoder = std::make_shared<V4l2Encoder>(config, std::move(output_callback));
            return std::move(encoder);
        }
        void PostCameraBuffer(std::shared_ptr<CameraBuffer> &&buffer);
        void Start();
        void Stop();
        V4l2Encoder(const EncoderConfig &config, SizedBufferCallback output_callback);
        ~V4l2Encoder();
    private:
        void setupEncoder();
        void setupUpstreamBuffers(unsigned int request_upstream_buffers);
        void setupDownstreamBuffers(unsigned int request_downstream_buffers);
        void run();
        void encodeBuffer(std::shared_ptr<CameraBuffer> &&buffer);
        bool waitForEncoder();
        void queueDownstreamBuffer(EncoderBuffer *e) const;
        void teardownUpstreamBuffers();
        void teardownDownstreamBuffers();

        static const char _device_name[];

        SizedBufferCallback _output_callback;
        const std::pair<int, int> _width_height;

        int _encoder_fd = -1;
        std::atomic<bool> _is_primed = false;

        std::mutex _available_upstream_buffers_mutex;
        std::queue<EncoderBuffer*> _available_upstream_buffers;

        void *_output_mem = nullptr;

        std::mutex _work_mutex;
        std::condition_variable _work_cv;
        std::queue<std::shared_ptr<CameraBuffer>> _work_queue;
        std::unique_ptr<std::thread> _work_thread;
        std::atomic<bool> _work_stop = { true };
        std::map<unsigned int, EncoderBuffer*> _downstream_buffers;
    };

}

#endif //INFRASTRUCTURE_ENCODER_V4L2_ENCODER_HPP

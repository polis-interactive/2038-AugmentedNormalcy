//
// Created by brucegoose on 4/12/23.
//

#ifndef INFRASTRUCTURE_DECODER_V4L2_DECODER_HPP
#define INFRASTRUCTURE_DECODER_V4L2_DECODER_HPP

#include <memory>
#include <queue>
#include <mutex>
#include <map>
#include <atomic>
#include <thread>
#include <iostream>

#include "utils/buffers.hpp"

namespace infrastructure {

    struct DecoderConfig {
        [[nodiscard]] virtual unsigned int get_decoder_upstream_buffer_count() const = 0;
        [[nodiscard]] virtual unsigned int get_decoder_downstream_buffer_count() const = 0;
        [[nodiscard]] virtual std::pair<int, int> get_decoder_width_height() const = 0;
    };

    class V4l2UpstreamBuffer: public ResizableBuffer {
    public:
        [[nodiscard]] virtual bool IsLeakyBuffer() = 0;
    };

    class V4l2ResizableBuffer: public V4l2UpstreamBuffer {
    public:
        V4l2ResizableBuffer(void *memory, unsigned int index, std::size_t max_size):
            _memory(memory), _index(index), _max_size(max_size), _used_size(max_size) {}
        [[nodiscard]] void *GetMemory() override {
            return _memory;
        };
        [[nodiscard]] std::size_t GetSize() override {
            return _used_size;
        };
        void SetSize(std::size_t used_size) override {
            _used_size = used_size;
        };
        [[nodiscard]] unsigned int GetIndex() const {
            return _index;
        }
        [[nodiscard]] unsigned int GetMaxSize() const {
            return _max_size;
        }
        [[nodiscard]] bool IsLeakyBuffer() final {
            return false;
        };
        ~V4l2ResizableBuffer() {
            std::cout << "AGH MAJOR PANIC" << std::endl;
        }
    private:
        void *_memory;
        const unsigned int _index;
        const std::size_t _max_size;
        std::size_t _used_size;
    };

    class V4l2LeakyUpstreamBuffer: public V4l2UpstreamBuffer {
    public:
        explicit V4l2LeakyUpstreamBuffer(const std::size_t buffer_size):
            _max_size(buffer_size)
        {
            _buffer = new unsigned char[buffer_size];
        }
        [[nodiscard]] void *GetMemory() override {
            return _buffer;
        };
        [[nodiscard]] std::size_t GetSize() override {
            return _used_size;
        };
        void SetSize(std::size_t used_size) override {
            _used_size = used_size;
        };
        void ResetSize() {
            _used_size = _max_size;
        };
        [[nodiscard]] bool IsLeakyBuffer() final {
            return true;
        };
    private:
        unsigned char *_buffer;
        std::size_t _used_size = 0;
        const std::size_t _max_size;
    };

    class V4l2Decoder: public std::enable_shared_from_this<V4l2Decoder>, public ResizableBufferPool {
    public:
        static std::shared_ptr<V4l2Decoder>Create(
            const DecoderConfig &config, DecoderBufferCallback output_callback
        ) {
            auto decoder = std::make_shared<V4l2Decoder>(config, std::move(output_callback));
            return std::move(decoder);
        }
        [[nodiscard]] std::shared_ptr<ResizableBuffer> GetResizableBuffer() override;
        void PostResizableBuffer(std::shared_ptr<ResizableBuffer> &&buffer) override;
        void PostResizableBuffers(V4l2ResizableBuffer *buffer);
        void PostVoidBuffer(std::shared_ptr<V4l2ResizableBuffer> &&buffer);
        void Start();
        void Stop();
        void Dummy();
        V4l2Decoder(const DecoderConfig &config, DecoderBufferCallback output_callback);
        ~V4l2Decoder();
    private:
        void setupDecoder(unsigned int request_upstream_buffers, unsigned int request_downstream_buffers);
        void setupUpstreamBuffers(unsigned int request_upstream_buffers);
        void setupDownstreamBuffers(unsigned int request_downstream_buffers);
        void handleDownstream();
        bool waitForDecoder();
        std::shared_ptr<DecoderBuffer> getDownstreamBuffer();
        void queueDownstreamBuffer(DecoderBuffer *d) const;
        void teardownUpstreamBuffers();
        void teardownDownstreamBuffers();

        static const char _device_name[];

        DecoderBufferCallback _output_callback;
        const std::pair<int, int> _width_height;

        int _decoder_fd = -1;
        std::atomic<bool> _decoder_running = false;
        std::atomic<bool> _is_primed = false;
        std::atomic<unsigned long> _timestamp = 0;

        std::mutex _available_upstream_buffers_mutex;
        std::queue<V4l2ResizableBuffer *> _available_upstream_buffers;
        std::map<unsigned int, V4l2ResizableBuffer *> _upstream_buffers;

        std::unique_ptr<std::thread> _downstream_thread;
        std::map<unsigned int, DecoderBuffer *> _downstream_buffers;

        std::shared_ptr<V4l2LeakyUpstreamBuffer> _leaky_upstream_buffer;
        std::vector<std::tuple<int, int, void *>> _output_params;
    };

}



#endif //INFRASTRUCTURE_DECODER_V4L2_DECODER_HPP

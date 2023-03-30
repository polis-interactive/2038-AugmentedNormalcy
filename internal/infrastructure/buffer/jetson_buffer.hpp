//
// Created by brucegoose on 3/28/23.
//

#ifndef INFRASTRUCTURE_BUFFER_JETSON_BUFFER_HPP
#define INFRASTRUCTURE_BUFFER_JETSON_BUFFER_HPP

#include <deque>
#include <mutex>

#include "buffer.hpp"

namespace infrastructure {

    class JetsonBuffer: public SizedBuffer {
    public:
        explicit JetsonBuffer(const unsigned int buffer_size);
        [[nodiscard]] void *GetMemory();
        [[nodiscard]] std::size_t GetSize() override {
            return _buffer_size;
        }
    private:
        const unsigned int _buffer_size;
    };

    class JetsonBufferPool: public SizedBufferPool, public std::enable_shared_from_this<JetsonBufferPool> {
    public:
        static std::shared_ptr<JetsonBufferPool> Create(const unsigned int buffer_size, const unsigned int buffer_count) {
            return std::make_shared<JetsonBufferPool>(buffer_size, buffer_count);
        }
        JetsonBufferPool(unsigned int buffer_size, unsigned int buffer_count);
        [[nodiscard]] std::shared_ptr<SizedBuffer> GetSizedBuffer() override;
        ~JetsonBufferPool();
    private:
        std::deque<JetsonBuffer *> _buffers;
        std::mutex _buffer_mutex;
    };

    class JetsonBufferMaker: public BufferMaker {
    public:
        explicit JetsonBufferMaker(const BufferConfig &config) :
            _buffer_count(config.get_buffer_count()), _buffer_size(config.get_buffer_size())
        {}
        std::shared_ptr<SizedBufferPool> GetNewPool() override;
    private:
        const unsigned int _buffer_count;
        const unsigned int _buffer_size;
    };
}

#endif //INFRASTRUCTURE_BUFFER_JETSON_BUFFER_HPP

//
// Created by brucegoose on 3/28/23.
//

#include "jetson_buffer.hpp"


namespace infrastructure {

    JetsonBufferPool::JetsonBufferPool(const unsigned int buffer_size, unsigned int buffer_count) {
        for (int i = 0; i < buffer_count; i++) {
            _buffers.push_back(new JetsonBuffer(buffer_size));
        }
    }

    std::shared_ptr<SizedBuffer> JetsonBufferPool::GetSizedBuffer() {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        auto jetson_buffer = _buffers.front();
        _buffers.pop_front();
        auto self(shared_from_this());
        auto buffer = std::shared_ptr<SizedBuffer>(
                (SizedBuffer *) jetson_buffer, [this, s = std::move(self), jetson_buffer](SizedBuffer *) mutable {
                    std::unique_lock<std::mutex> lock(_buffer_mutex);
                    _buffers.push_back(jetson_buffer);
                }
        );
        return std::move(buffer);
    }

    JetsonBufferPool::~JetsonBufferPool() {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        while (!_buffers.empty()) {
            auto jetson_buffer = _buffers.front();
            _buffers.pop_front();
            delete jetson_buffer;
        }
    }

    std::shared_ptr<SizedBufferPool> JetsonBufferMaker::GetNewPool() {
        return JetsonBufferPool::Create(_buffer_size, _buffer_count);
    }
}
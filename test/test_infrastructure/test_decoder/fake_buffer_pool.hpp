//
// Created by brucegoose on 5/7/23.
//

#ifndef AUGMENTEDNORMALCY_FAKE_BUFFER_POOL_HPP
#define AUGMENTEDNORMALCY_FAKE_BUFFER_POOL_HPP


class DecoderSizedBuffer: public SizedBuffer {
public:
    explicit DecoderSizedBuffer(unsigned int buffer_size):
            _buffer_size(buffer_size)
    {
        _buffer = new char[buffer_size];
    };
    [[nodiscard]] void *GetMemory() override {
        return _buffer;
    };
    [[nodiscard]] std::size_t GetSize() override {
        return _buffer_size;
    };
    char *_buffer;
    const unsigned int _buffer_size;
};

class DecoderSizedBufferPool: public SizedBufferPool {
public:
    explicit DecoderSizedBufferPool(
            unsigned int buffer_size, unsigned int buffer_count
    ){
        for (int i = 0; i < buffer_count; i++) {
            _buffers.push_back(new DecoderSizedBuffer(buffer_size));
        }
    }
    std::shared_ptr<SizedBuffer> GetSizedBuffer() override {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        auto fake_buffer = _buffers.front();
        _buffers.pop_front();
        auto buffer = std::shared_ptr<SizedBuffer>(
                (SizedBuffer *) fake_buffer, [this, fake_buffer](SizedBuffer *) mutable {
                    std::unique_lock<std::mutex> lock(_buffer_mutex);
                    _buffers.push_back(fake_buffer);
                }
        );
        return std::move(buffer);
    }
    std::size_t AvailableBuffers() {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        return _buffers.size();
    }
    std::deque<DecoderSizedBuffer *> _buffers;
    std::mutex _buffer_mutex;
};


#endif //AUGMENTEDNORMALCY_FAKE_BUFFER_POOL_HPP

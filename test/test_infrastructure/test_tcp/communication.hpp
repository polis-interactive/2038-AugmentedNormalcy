//
// Created by brucegoose on 3/18/23.
//

#ifndef AUGMENTEDNORMALCY_TEST_TCP_COMMUNICATION_HPP
#define AUGMENTEDNORMALCY_TEST_TCP_COMMUNICATION_HPP

#include <utility>
#include <deque>

#include "utils/buffers.hpp"


class FakeSizedBuffer: public SizedBuffer {
public:
    explicit FakeSizedBuffer(unsigned int buffer_size):
        _buffer_size(buffer_size)
    {
        _buffer = new char[buffer_size];
    };
    explicit FakeSizedBuffer(std::string &str):
        _buffer(new char[str.size()]),
        _buffer_size(str.size())
    {
        str.copy(_buffer, _buffer_size);
    }
    [[nodiscard]] void *GetMemory() override {
        return _buffer;
    };
    [[nodiscard]] std::size_t GetSize() override {
        return _buffer_size;
    };
    char *_buffer;
    const unsigned int _buffer_size;
};

class FakeSizedBufferPool: public SizedBufferPool {
public:
    explicit FakeSizedBufferPool(
            unsigned int buffer_size, unsigned int buffer_count
    ){
        for (int i = 0; i < buffer_count; i++) {
            _buffers.push_back(new FakeSizedBuffer(buffer_size));
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
    std::deque<FakeSizedBuffer *> _buffers;
    std::mutex _buffer_mutex;
};

class FakePlaneBuffer: public SizedBufferPool {
public:
    explicit FakePlaneBuffer(unsigned int buffer_size) {
        _buffer = std::make_shared<FakeSizedBuffer>(buffer_size);
    }
    explicit FakePlaneBuffer(std::string &str) {
        _buffer = std::make_shared<FakeSizedBuffer>(str);
    }
    [[nodiscard]] std::shared_ptr<SizedBuffer> GetSizedBuffer() override {
        if (_has_sent) {
            _has_sent = !_has_sent;
            return nullptr;
        } else {
            _has_sent = !_has_sent;
            return _buffer;
        }
    }
    bool _has_sent = false;
    std::shared_ptr<FakeSizedBuffer> _buffer;
};

class FakePlaneBufferPool: public SizedPlaneBufferPool {
public:
    explicit FakePlaneBufferPool(
        unsigned int buffer_size, unsigned int buffer_count, SizedBufferPoolCallback callback
    ):
        _callback(std::move(callback))
    {
        for (int i = 0; i < buffer_count; i++) {
            _buffers.push_back(new FakePlaneBuffer(buffer_size));
        }
    }
    std::shared_ptr<SizedBufferPool> GetSizedBufferPool() override {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        auto fake_buffer = _buffers.front();
        _buffers.pop_front();
        auto buffer = std::shared_ptr<SizedBufferPool>(
            (SizedBufferPool *) fake_buffer, [this, fake_buffer](SizedBufferPool *) mutable {
                std::unique_lock<std::mutex> lock(_buffer_mutex);
                _buffers.push_back(fake_buffer);
            }
        );
        return std::move(buffer);
    }
    void PostSizedBufferPool(std::shared_ptr<SizedBufferPool> &&buffer) override {
        _callback(std::move(buffer));
    }
    std::size_t AvailableBuffers() {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        return _buffers.size();
    }
    std::deque<FakePlaneBuffer *> _buffers;
    std::mutex _buffer_mutex;
    SizedBufferPoolCallback _callback;
};


class FakeResizableBuffer: public ResizableBuffer {
public:
    explicit FakeResizableBuffer(unsigned int max_buffer_size):
            _max_buffer_size(max_buffer_size),
            _buffer_size(max_buffer_size)
    {
        _buffer = new char[_max_buffer_size];
    };
    [[nodiscard]] void *GetMemory() override {
        return _buffer;
    };
    [[nodiscard]] std::size_t GetSize() override {
        return _buffer_size;
    };
    void SetSize(std::size_t used_size) override {
        _buffer_size = used_size;
    };
    [[nodiscard]] bool IsLeakyBuffer() override {
        return false;
    };
    char *_buffer;
    unsigned int _buffer_size;
    const unsigned int _max_buffer_size;
};

class FakeResizableBufferPool: public ResizableBufferPool {
public:
    explicit FakeResizableBufferPool(
        unsigned int max_buffer_size, unsigned int buffer_count, ResizableBufferCallback callback
    ):
            _callback(std::move(callback))
    {
        for (int i = 0; i < buffer_count; i++) {
            _buffers.push_back(new FakeResizableBuffer(max_buffer_size));
        }
    }
    [[nodiscard]] std::shared_ptr<ResizableBuffer> GetResizableBuffer() override {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        auto fake_buffer = _buffers.front();
        _buffers.pop_front();
        auto buffer = std::shared_ptr<ResizableBuffer>(
            (ResizableBuffer *) fake_buffer, [this, fake_buffer](ResizableBuffer *) mutable {
                std::unique_lock<std::mutex> lock(_buffer_mutex);
                _buffers.push_back(fake_buffer);
            }
        );
        return std::move(buffer);
    };
    void PostResizableBuffer(std::shared_ptr<ResizableBuffer> &&buffer) override {
        _callback(std::move(buffer));
    };
    std::size_t AvailableBuffers() {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        return _buffers.size();
    }
    std::deque<FakeResizableBuffer *> _buffers;
    std::mutex _buffer_mutex;
    ResizableBufferCallback _callback;
};



#endif //AUGMENTEDNORMALCY_TEST_TCP_COMMUNICATION_HPP

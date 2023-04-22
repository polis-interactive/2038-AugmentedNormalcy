//
// Created by brucegoose on 3/18/23.
//

#ifndef AUGMENTEDNORMALCY_UTILS_BUFFERS_BASE_HPP
#define AUGMENTEDNORMALCY_UTILS_BUFFERS_BASE_HPP

#include <memory>
#include <functional>

struct SizedBuffer {
    [[nodiscard]] virtual void *GetMemory() = 0;
    [[nodiscard]] virtual std::size_t GetSize() = 0;
};

using SizedBufferCallback = std::function<void(std::shared_ptr<SizedBuffer>&&)>;

class SizedBufferPool {
public:
    [[nodiscard]] virtual std::shared_ptr<SizedBuffer> GetSizedBuffer() = 0;
};

using SizedBufferPoolCallback = std::function<void(std::shared_ptr<SizedBufferPool>&&)>;

class SizedPlaneBufferPool {
public:
    /* TODO: get rid of super hack */
    virtual void Start() {};
    virtual void Stop() {};
    [[nodiscard]] virtual std::shared_ptr<SizedBufferPool> GetSizedBufferPool() = 0;
    virtual void PostSizedBufferPool(std::shared_ptr<SizedBufferPool> &&buffer) = 0;
};

struct ResizableBuffer {
    [[nodiscard]] virtual void *GetMemory() = 0;
    [[nodiscard]] virtual std::size_t GetSize() = 0;
    virtual void SetSize(std::size_t used_size) = 0;
    [[nodiscard]] virtual bool IsLeakyBuffer() = 0;
};

using ResizableBufferCallback = std::function<void(std::shared_ptr<ResizableBuffer>&&)>;

class ResizableBufferPool {
public:
    [[nodiscard]] virtual std::shared_ptr<ResizableBuffer> GetResizableBuffer() = 0;
    virtual void PostResizableBuffer(std::shared_ptr<ResizableBuffer> &&buffer) = 0;
};

// nowhere better to put this at the moment

struct DecoderBuffer: public SizedBuffer {
    DecoderBuffer(unsigned int buffer_index, int fd, void *memory, std::size_t size):
        _buffer_index(buffer_index),
        _fd(fd),
        _memory(memory),
        _size(size)
    {}

    [[nodiscard]] void *GetMemory() override {
        return _memory;
    };
    [[nodiscard]] int GetFd() const {
        return _fd;
    }
    [[nodiscard]] std::size_t GetSize() override {
        return _size;
    };
    [[nodiscard]] std::size_t GetIndex() const {
        return _buffer_index;
    };
private:
    const unsigned int _buffer_index;
    const int _fd;
    void *_memory;
    std::size_t _size;
};

using DecoderBufferCallback = std::function<void(std::shared_ptr<DecoderBuffer>&&)>;


#endif //AUGMENTEDNORMALCY_UTILS_BUFFERS_BASE_HPP

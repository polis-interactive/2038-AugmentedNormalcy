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
    [[nodiscard]] virtual std::shared_ptr<SizedBufferPool> GetSizedBufferPool() = 0;
    virtual void PostSizedBufferPool(std::shared_ptr<SizedBufferPool> &&buffer) = 0;
};

// nowhere better to put this at the moment

struct CameraBuffer: public SizedBuffer {
    CameraBuffer(
        void *request, void *buffer, int fd, std::size_t size, int64_t timestamp_us
    ): _request(request), _buffer(buffer), _fd(fd), _size(size), _timestamp_us(timestamp_us) {}
    [[nodiscard]] void *GetMemory() final {
        return _buffer;
    }
    [[nodiscard]] void *GetRequest() const {
        return _request;
    }
    [[nodiscard]] int GetFd() const {
        return _fd;
    }
    [[nodiscard]] std::size_t GetSize() final {
        return _size;
    };
private:
    void *_request;
    void *_buffer;
    int _fd;
    std::size_t _size;
    int64_t _timestamp_us;
};


#endif //AUGMENTEDNORMALCY_UTILS_BUFFERS_BASE_HPP

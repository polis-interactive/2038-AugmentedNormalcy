//
// Created by bruce on 12/31/2022.
//

#ifndef INFRASTRUCTURE_COMMON_HPP
#define INFRASTRUCTURE_COMMON_HPP

#include <array>
#include <tuple>
#include <cmath>
#include "utility/buffer_pool.hpp"

#define MAX_FRAME_LENGTH 64000
#define MAX_REPLY_LENGTH 1024

struct SizedBuffer {
    virtual void *GetMemory() = 0;
    virtual std::size_t GetSize() = 0;
};

struct CameraBuffer: public SizedBuffer {
    CameraBuffer(void *buffer, int fd, std::size_t size): _buffer(buffer), _fd(fd), _size(size) {}
    void *GetMemory()  {
        return _buffer;
    }
    std::size_t GetSize() {
        return _size;
    };
    int GetFd() {
        return _fd;
    }
    void *_buffer;
    int _fd;
    std::size_t _bytes_used;
    std::size_t _size;

};


using PayloadBuffer = std::array<uint8_t, MAX_FRAME_LENGTH>;
struct SizedPayloadBuffer : public SizedBuffer {
    PayloadBuffer _buffer{};
    std::size_t _size{};
    void *GetMemory() final {
        return _buffer.data();
    }
    std::size_t GetSize() final {
        return _size;
    }
};
using PayloadBufferPool = utility::BufferPool<SizedPayloadBuffer>;

using reply_buffer = std::array<uint8_t, MAX_REPLY_LENGTH>;
using reply_buffer_pool = utility::BufferPool<reply_buffer>;


const auto rolling_less_than = [](const uint16_t &a, const uint16_t &b) {
    // a == b, not less than
    // a < b if b - a is < 2^(16 - 1) (less than a big roll-over)
    // a > b if a - b is < 2^(16 - 1) (a is bigger but there is a huge roll-over)
    return a != b and (
            (a < b and (b - a) < std::pow(2, 16 - 1)) or
            (a > b and (a - b) > std::pow(2,16 - 1))
    );
};


#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif


#endif //INFRASTRUCTURE_COMMON_HPP

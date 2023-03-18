//
// Created by brucegoose on 3/18/23.
//

#ifndef AUGMENTEDNORMALCY_BUFFER_POOL_HPP
#define AUGMENTEDNORMALCY_BUFFER_POOL_HPP

#include <memory>

struct SizedBuffer {
    [[nodiscard]] virtual void *GetMemory() = 0;
    [[nodiscard]] virtual std::size_t GetSize() = 0;
};

class PushingBufferPool {
public:
    [[nodiscard]] virtual std::shared_ptr<SizedBuffer> GetSizedBuffer() = 0;
    virtual void SendSizedBuffer(std::shared_ptr<SizedBuffer> &&buffer) = 0;
};

#endif //AUGMENTEDNORMALCY_BUFFER_POOL_HPP

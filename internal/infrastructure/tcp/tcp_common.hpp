//
// Created by brucegoose on 3/16/23.
//

#ifndef AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_COMMON_HPP
#define AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_COMMON_HPP

#include <memory>

struct SizedBuffer {
    [[nodiscard]] virtual void *GetMemory() = 0;
    [[nodiscard]] virtual std::size_t GetSize() = 0;
};

class PushingBufferPool {
public:
    [[nodiscard]] virtual std::shared_ptr<SizedBuffer> &&GetSizedBuffer() = 0;
    virtual void SendSizedBuffer(std::shared_ptr<SizedBuffer> &&buffer) = 0;
};

#endif //AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_COMMON_HPP

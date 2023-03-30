//
// Created by brucegoose on 3/28/23.
//

#ifndef INFRASTRUCTURE_BUFFER_HPP
#define INFRASTRUCTURE_BUFFER_HPP


#include "utils/buffers.hpp"

namespace infrastructure {

    enum class BufferType {
        JETSON,
        ARRAY,
    };

    struct BufferConfig {
        [[nodiscard]] virtual unsigned int get_buffer_size() const = 0;
        [[nodiscard]] virtual unsigned int get_buffer_count() const = 0;
        [[nodiscard]] virtual BufferType get_buffer_type() const = 0;
    };

    class BufferMaker {
    public:
        [[nodiscard]] static std::shared_ptr<BufferMaker> Create(const BufferConfig &config);
        virtual std::shared_ptr<SizedBufferPool> GetNewPool() = 0;
    };
}

#endif //INFRASTRUCTURE_BUFFER_HPP

//
// Created by brucegoose on 3/28/23.
//

#ifndef INFRASTRUCTURE_BUFFER_ARRAY_BUFFER_HPP
#define INFRASTRUCTURE_BUFFER_ARRAY_BUFFER_HPP

#include "buffer.hpp"

namespace infrastructure {

    class ArrayBufferMaker: public BufferMaker {
    public:
        explicit ArrayBufferMaker(const BufferConfig &config) :
            _buffer_count(config.get_buffer_count()), _buffer_size(config.get_buffer_size())
        {}
        std::shared_ptr<SizedBufferPool> GetNewPool();
    private:
        const unsigned int _buffer_count;
        const unsigned int _buffer_size;
    };
}

#endif //INFRASTRUCTURE_BUFFER_ARRAY_BUFFER_HPP

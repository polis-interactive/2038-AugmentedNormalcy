//
// Created by brucegoose on 3/28/23.
//

#include "buffer.hpp"

#ifdef _JETSON_BUFFER_
#include "jetson_buffer.hpp"
#endif

#include "array_buffer.hpp"


namespace infrastructure {
    std::shared_ptr<BufferMaker> BufferMaker::Create(const BufferConfig &config) {
        switch(config.get_buffer_type()) {
#ifdef _JETSON_BUFFER_
            case BufferType::JETSON:
                return std::make_shared<JetsonBufferMaker>(config);
#endif
            case BufferType::ARRAY:
                return std::make_shared<ArrayBufferMaker>(config);
            default:
                throw std::runtime_error("Selected camera unavailable... ");
        }
    }
}
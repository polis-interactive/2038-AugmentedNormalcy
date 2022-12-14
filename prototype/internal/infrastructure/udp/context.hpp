//
// Created by brucegoose on 12/10/22.
//

#ifndef AUGMENTEDNORMALCY_PROTOTYPE_CONTEXT_CPP
#define AUGMENTEDNORMALCY_PROTOTYPE_CONTEXT_CPP

#include <memory>
#include <boost/asio.hpp>

struct UdpContextConfig {
    virtual int get_pool_size() const = 0;
    virtual int get_buffer_size() const = 0;
};

class UdpContext {
public:
    UdpContext(const UdpContextConfig &config):
        _buffer_size(config.get_buffer_size()),
        _pool(std::make_shared<boost::asio::thread_pool>(config.get_pool_size()))
    {}
    ~UdpContext() {
        if (_pool != nullptr) {
            _pool->join();
        }
    }
protected:
    const int _buffer_size;
    std::shared_ptr<boost::asio::thread_pool> _pool;
};

#endif //AUGMENTEDNORMALCY_PROTOTYPE_CONTEXT_CPP

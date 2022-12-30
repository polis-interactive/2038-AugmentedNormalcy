//
// Created by brucegoose on 12/10/22.
//

#ifndef AUGMENTEDNORMALCY_PROTOTYPE_CONTEXT_CPP
#define AUGMENTEDNORMALCY_PROTOTYPE_CONTEXT_CPP

#include <memory>
#include <boost/asio.hpp>
#include <thread>

namespace net = boost::asio;

namespace infrastructure {
    struct UdpContextConfig {
        [[nodiscard]] virtual int get_udp_pool_size() const = 0;
    };

    class UdpContext {
    public:
        explicit UdpContext(const UdpContextConfig &config) :
                _context(net::io_context(config.get_udp_pool_size())),
                _pool(std::make_unique<std::vector<std::jthread>>(config.get_udp_pool_size())){}

        void Start() noexcept {
            std::generate(
                    _pool->begin(),
                    _pool->end(),
                    [&context = this->_context]()-> std::jthread {
                        return std::jthread([&context] { context.run(); });
                    }
            );
        }

        void Stop() {
            if (!_context.stopped()) {
                _context.stop();
                if (_pool) {
                    for (auto &thread : *_pool) {
                        thread.request_stop();
                        thread.join();
                    }
                    _pool.reset();
                }
                _context.reset();
            }
        }
        // should be marked friend and make this protected, or just use the member directly
        net::io_context &GetContext() {
            return _context;
        }
    private:
        net::io_context _context;
        std::unique_ptr<std::vector<std::jthread>> _pool = {nullptr};
    };
}

#endif //AUGMENTEDNORMALCY_PROTOTYPE_CONTEXT_CPP

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
                _context(std::make_shared<net::io_context>(config.get_udp_pool_size())),
                _pool(std::vector<std::jthread>(config.get_udp_pool_size())){}

        void Start() noexcept {
            _context->restart();
            std::generate(
                    _pool.begin(),
                    _pool.end(),
                    [context = this->_context]()-> std::jthread {
                        return std::jthread([context](std::stop_token st) {
                            while (!st.stop_requested()) {
                                context->run_one_for(500ms);
                            }
                        });
                    }
            );
        }

        void Stop() {
            if (!_context->stopped()) {
                _context->stop();
                if (!_pool.empty()) {
                    for (auto &thread : _pool) {
                        thread.request_stop();
                        thread.join();
                    }
                    _pool.clear();
                }
            }
        }
        // should be marked friend and make this protected, or just use the member directly
        std::shared_ptr<net::io_context> GetContext() {
            return _context;
        }
    private:
        std::shared_ptr<net::io_context> _context;
        std::vector<std::jthread> _pool;
    };
}

#endif //AUGMENTEDNORMALCY_PROTOTYPE_CONTEXT_CPP

//
// Created by brucegoose on 3/5/23.
//

#ifndef AUGMENTEDNORMALCY_INFRASTRUCTURE_TCPCONTEXT_HPP
#define AUGMENTEDNORMALCY_INFRASTRUCTURE_TCPCONTEXT_HPP

#include <memory>
#include <boost/asio.hpp>
#include <thread>
#include <iostream>

namespace net = boost::asio;

struct AsioContextConfig {
    [[nodiscard]] virtual int get_asio_pool_size() const = 0;
};

class AsioContext: public std::enable_shared_from_this<AsioContext> {
public:

    static std::shared_ptr<AsioContext> Create(const AsioContextConfig &config) {
        return std::make_shared<AsioContext>(config);
    }

    explicit AsioContext(const AsioContextConfig &config) :
            _context(config.get_asio_pool_size()),
            _guard(net::make_work_guard(_context)),
            _pool(std::vector<std::thread>(config.get_asio_pool_size()))
    {}

    void Start() noexcept {
        if (_started) {
            return;
        }
        _started = true;
        _stop = false;
        std::generate(
                _pool.begin(),
                _pool.end(),
                [&context = this->_context, &stop=this->_stop]()-> std::thread {
                    return std::thread([&context, &stop]() {
                        while (!stop) {
                            try {
                                context.run();
                            } catch (std::exception const &e) {
                                std::cout << e.what() << std::endl;
                            } catch (...) {
                                std::cout << "WTF Error" << std::endl;
                            }
                        }
                    });
                }
        );
    }

    void Stop() {
        if (!_started) {
            return;
        }
        if (!_context.stopped()) {
            _guard.reset();
            _context.stop();
        }
        if (!_stop) {
            _stop = true;
            if (!_pool.empty()) {
                for (auto &thread : _pool) {
                    thread.join();
                }
                _pool.clear();
            }
        }
        _started = false;
    }
    // should be marked friend and make this protected, or just use the member directly
    net::io_context &GetContext() {
        return _context;
    }
    ~AsioContext() {
        Stop();
    }
private:

    net::io_context _context;
    net::executor_work_guard<net::io_context::executor_type> _guard;
    std::vector<std::thread> _pool;
    std::atomic<bool> _stop = { false };
    std::atomic<bool> _started = { false };
};

#endif //AUGMENTEDNORMALCY_INFRASTRUCTURE_TCPCONTEXT_HPP

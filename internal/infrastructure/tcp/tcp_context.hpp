//
// Created by brucegoose on 3/5/23.
//

#ifndef AUGMENTEDNORMALCY_INFRASTRUCTURE_TCPCONTEXT_HPP
#define AUGMENTEDNORMALCY_INFRASTRUCTURE_TCPCONTEXT_HPP

#include <memory>
#include <boost/asio.hpp>
#include <thread>
#include <iostream>

#include "utils/buffers.hpp"

namespace net = boost::asio;

typedef net::detail::socket_option::boolean<SOL_SOCKET, SO_REUSEPORT> reuse_port;

namespace infrastructure {


    class TcpReaderMessage
    {
    public:
        static constexpr std::size_t header_length = 16;
        static constexpr std::size_t max_body_length = 786432;

        TcpReaderMessage(): _body_length(0)
        {
        }

        [[nodiscard]] const char* Data() const
        {
            return _data;
        }

        char* Data()
        {
            return _data;
        }

        std::size_t BodyLength() const
        {
            return _body_length;
        }


        bool DecodeHeader()
        {
            char header[header_length + 1] = "";
            std::strncat(header, _data, header_length);
            _body_length = std::atoi(header);
            if (_body_length > max_body_length)
            {
                _body_length = 0;
                return false;
            }
            return true;
        }

        [[nodiscard]] std::size_t Length() const
        {
            return header_length;
        }


    private:
        char _data[header_length];
        std::size_t _body_length;
    };

    class TcpWriterMessage
    {
    public:

        static constexpr std::size_t header_length = 16;
        static constexpr std::size_t max_body_length = 786432;

        explicit TcpWriterMessage(std::shared_ptr<SizedBuffer> &&buffer):
            _buffer(std::move(buffer))
        {
            encodeHeader();
        }

        [[nodiscard]] std::shared_ptr<SizedBuffer> &GetBuffer() {
            return _buffer;
        }

        char* Data()
        {
            return _data;
        }

        [[nodiscard]] std::size_t Length() const
        {
            return header_length;
        }
    private:
        void encodeHeader()
        {
            char header[header_length + 1] = "";
            std::sprintf(header, "%16d", static_cast<int>(_buffer->GetSize()));
            std::memcpy(_data, header, header_length);
        }

        char _data[header_length]{};
        std::shared_ptr<SizedBuffer> _buffer;
    };

    struct TcpContextConfig {
        [[nodiscard]] virtual int get_tcp_pool_size() const = 0;
    };

    class TcpContext: public std::enable_shared_from_this<TcpContext> {
    public:

        static std::shared_ptr<TcpContext> Create(const TcpContextConfig &config) {
            return std::make_shared<TcpContext>(config);
        }

        explicit TcpContext(const TcpContextConfig &config) :
                _context(config.get_tcp_pool_size()),
                _guard(net::make_work_guard(_context)),
                _pool(std::vector<std::thread>(config.get_tcp_pool_size()))
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
        ~TcpContext() {
            Stop();
        }
    private:

        net::io_context _context;
        net::executor_work_guard<net::io_context::executor_type> _guard;
        std::vector<std::thread> _pool;
        std::atomic<bool> _stop = { false };
        std::atomic<bool> _started = { false };
    };
}

#endif //AUGMENTEDNORMALCY_INFRASTRUCTURE_TCPCONTEXT_HPP

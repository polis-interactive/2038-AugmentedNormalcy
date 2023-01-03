//
// Created by bruce on 12/29/2022.
//

#ifndef AUGMENTEDNORMALCY_PROTOTYPE_CLIENT_HPP
#define AUGMENTEDNORMALCY_PROTOTYPE_CLIENT_HPP

#include <map>
#include <memory>
#include <chrono>
#include "boost/asio.hpp"

#include "infrastructure/udp/common.hpp"

#include <iostream>
#include <utility>


namespace infrastructure {

    struct UdpClientPoolConfig {
        [[nodiscard]] virtual int get_udp_server_port() const = 0;
        [[nodiscard]] virtual std::chrono::milliseconds get_udp_rw_timeout() const = 0;
    };

    class UdpClient;

    class UdpClientPool {
    public:
        UdpClientPool(const UdpClientPoolConfig &config, net::io_context &context) :
            _context(context),
            _port(config.get_udp_server_port())
        {}
        [[nodiscard]] std::shared_ptr<UdpClient> GetOrCreateClient(const std::string& address, payload_buffer_pool &pool);
        void Stop();
    protected:
        net::io_context &_context;
        int _port;
        friend UdpClient;
    private:
        std::map<udp::endpoint, std::shared_ptr<UdpClient>> _clients;
        std::mutex _mutex;
    };

    class UdpClient: PayloadSend {
    public:
        UdpClient(
            net::io_context &context, udp::endpoint remote, payload_buffer_pool &b_pool
        ) :
            _strand(context),
            _remote(std::move(remote)),
            _socket(context, udp::endpoint(udp::v4(), 0)),
            _b_pool(b_pool)
        {
            Receive();
        }
        std::shared_ptr<payload_buffer> GetBuffer() override {
            return std::move(_b_pool.New());
        }
        void Send(std::shared_ptr<payload_buffer> &&buffer, std::size_t buffer_size) override {
            _socket.async_send_to(
                net::buffer(*buffer, buffer_size), _remote,
                net::bind_executor(_strand, [buffer, &pool = this->_b_pool] (error_code ec, std::size_t bytes_send) mutable {
                    pool.Free(std::move(buffer));
                })
            );
        }
        void Stop() {
            if (_socket.is_open()) {
                _socket.close();
            }
        }
    private:
        // we should have a received buffer pool here
        void Receive() {
            _socket.async_receive_from(
                net::buffer(_receive_buffer, MAX_REPLY_LENGTH), _remote,
                net::bind_executor(_strand, [this](error_code ec, std::size_t bytes_received) {
                    if (!ec && bytes_received > 0) {
                        net::post(_strand, [this, bytes_received](){
                            const auto s = std::string(
                                std::begin(_receive_buffer), std::begin(_receive_buffer) + bytes_received
                            );
                            std::cout << s << std::endl;
                        });
                    }
                    if (ec != net::error::bad_descriptor) {
                        Receive();
                    }
                })
            );
        }
        udp_socket _socket;
        net::io_context::strand _strand;
        udp::endpoint _remote;
        reply_buffer _receive_buffer{};
        payload_buffer_pool &_b_pool;
    };

}

#endif //AUGMENTEDNORMALCY_PROTOTYPE_CLIENT_HPP

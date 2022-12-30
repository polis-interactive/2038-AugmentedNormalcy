//
// Created by bruce on 12/29/2022.
//

#ifndef AUGMENTEDNORMALCY_PROTOTYPE_SERVER_HPP
#define AUGMENTEDNORMALCY_PROTOTYPE_SERVER_HPP

#include <memory>
#include <boost/asio.hpp>
#include <utility>
#include "utility/buffer_pool.hpp"
#include "constants.hpp"

namespace net = boost::asio;
using boost::asio::ip::udp;
using boost::system::error_code;


namespace infrastructure {

    struct UdpServerConfig {
        [[nodiscard]] virtual int get_udp_server_port() const = 0;
        [[nodiscard]] virtual int get_udp_server_buffer_count() const = 0;
    };

    // move semantics here a little scuffed...
    struct UdpServerSession {
        UdpServerSession(udp_buffer_pool &pool, net::io_context::strand &write_strand) :
            _buffer(pool.New()),
            _write_strand(write_strand),
            _on_destroy([&pool](std::shared_ptr<udp_buffer> &&buffer) {
                pool.Free(std::move(buffer));
            })
        {}
        ~UdpServerSession() {
            _on_destroy(std::move(_buffer));
        }
        udp::endpoint _remote;
        net::io_context::strand &_write_strand;
        std::shared_ptr<udp_buffer> _buffer;
        std::size_t _bytes_received = 0;
        std::function <void(std::shared_ptr<udp_buffer> &&)> _on_destroy;
    };

    class UdpServer {
    public:
        UdpServer(
            const UdpServerConfig &config, net::io_context &context,
            std::function<void(std::shared_ptr<UdpServerSession>)> &&receive_callback
        ) :
            _context(context),
            _socket(std::make_shared<udp::socket>(
                _context, udp::endpoint(udp::v4(), config.get_udp_server_port()))
            ),
            _read_strand(context),
            _process_strand(context),
            _write_strand(context),
            _pool(config.get_udp_server_buffer_count(), [](){
                return std::make_shared<std::array<uint8_t, MAX_PACKET_LENGTH>>();
            }),
            _receive_callback(std::move(receive_callback))
        {}
        void Start() {
            Receive();
        }
        void Stop() {
            _socket->close();
        }

    private:
        void Receive() {
            auto session = std::make_shared<UdpServerSession>(_pool, _write_strand);
            _socket->async_receive_from(
                net::buffer(*session->_buffer, MAX_PACKET_LENGTH),
                session->_remote,
                net::bind_executor(_read_strand, [session, this](error_code ec, std::size_t bytes_received) {
                    if (!ec && bytes_received > 0) {
                        session->_bytes_received = bytes_received;
                        net::post(_process_strand, [session, callback = this->_receive_callback](){
                            callback(session);
                        });
                    }
                    Receive();
                })
            );
        }
        net::io_context::strand _read_strand;
        net::io_context::strand _process_strand;
        net::io_context::strand _write_strand;
        udp_buffer_pool _pool;
        net::io_context &_context;
        std::shared_ptr<udp::socket> _socket;
        std::function<void(std::shared_ptr<UdpServerSession>)> _receive_callback;
    };

}

#endif //AUGMENTEDNORMALCY_PROTOTYPE_SERVER_HPP

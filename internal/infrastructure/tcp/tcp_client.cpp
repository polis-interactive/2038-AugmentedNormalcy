//
// Created by brucegoose on 3/16/23.
//

#include "tcp_client.hpp"

#include <chrono>
using namespace std::literals;


namespace infrastructure {
    TcpClient::TcpClient(
        const infrastructure::TcpClientConfig &config, net::io_context &context,
        std::shared_ptr<TcpClientManager> &manager
    ):
        _context(context),
        _remote_endpoint(
            net::ip::address::from_string(config.get_tcp_server_host()),
            config.get_tcp_server_port()
        ),
        _socket(net::make_strand(context)),
        _manager(manager),
        _is_camera(config.get_tcp_client_is_camera())
    {}

    void TcpClient::Start() {
        if (_is_stopped) {
            startConnection(true);
        }
    }

    void TcpClient::Stop() {
        if (!_is_stopped) {
            _is_stopped = true;
            boost::asio::post(
                net::make_strand(_context),
                [this]() {
                    error_code ec;
                    disconnect(ec);
                }
            );
        }
    }

    void TcpClient::startConnection(const bool is_initial_connection) {
        if (!is_initial_connection) {
            if (_is_stopped) return;
            std::this_thread::sleep_for(1s);
        }
        if (_is_stopped) return;
        _socket.async_connect(
            _remote_endpoint,
            [this](error_code ec) {
                if (!ec && !_is_stopped) {
                    _is_connected = true;
                    if (_is_camera) {
                        startWrite();
                    } else {
                        startRead();
                    }
                } else if (!_is_stopped) {
                    startConnection(false);
                }
            }
        );
    }

    void TcpClient::startWrite() {
       _manager->CreateCameraConnection(
           [this](std::shared_ptr<SizedBuffer> &&buffer) {
               write(std::move(buffer));
           }
       );
    }

    void TcpClient::write(std::shared_ptr<SizedBuffer> &&buffer) {
        if (_is_stopped || !_is_connected) return;
        auto buffer_memory = buffer->GetMemory();
        auto buffer_size = buffer->GetSize();
        _socket.async_send(
            net::buffer(buffer_memory, buffer_size),
            [this, send_buffer = std::move(buffer)](error_code ec, std::size_t bytes_written) {
                if (ec || bytes_written != send_buffer->GetSize()) {
                    reconnect(ec);
                }
            }
        );
    }

    void TcpClient::startRead() {
        _buffer_pool = _manager->CreateHeadsetConnection();
        net::dispatch(
            _socket.get_executor(),
            [this]() {
                read();
            }
        );
    }

    void TcpClient::read() {
        if (_is_stopped || !_is_connected) return;
        auto buffer = _buffer_pool->GetSizedBuffer();
        auto buffer_memory = buffer->GetMemory();
        auto buffer_size = buffer->GetSize();
        _socket.async_receive(
            net::buffer(buffer_memory, buffer_size),
            [this, camera_buffer = std::move(buffer)] (error_code ec, std::size_t bytes_written) mutable {
                if (!ec && bytes_written == camera_buffer->GetSize() && !_is_stopped) {
                    _buffer_pool->SendSizedBuffer(std::move(camera_buffer));
                    read();
                } else {
                    reconnect(ec);
                }
            }
        );
    }

    void TcpClient::reconnect(error_code ec) {
        disconnect(ec);
        startConnection(false);
    }

    void TcpClient::disconnect(error_code ec) {
        _is_connected = false;
        if (_socket.is_open()) {
            _socket.shutdown(tcp::socket::shutdown_both, ec);
        }
        if (_is_camera) {
            _manager->DestroyCameraConnection();
        } else {
            _manager->DestroyHeadsetConnection();
        }
    }
}
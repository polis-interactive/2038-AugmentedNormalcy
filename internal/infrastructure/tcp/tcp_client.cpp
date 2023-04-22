//
// Created by brucegoose on 3/16/23.
//

#include "tcp_client.hpp"

#include <chrono>
using namespace std::literals;


namespace infrastructure {
    TcpClient::TcpClient(
        const infrastructure::TcpClientConfig &config, net::io_context &context,
        std::shared_ptr<TcpClientManager> manager
    ):
        _context(context),
        _remote_endpoint(
            net::ip::address::from_string(config.get_tcp_server_host()),
            config.get_tcp_server_port()
        ),
        _manager(std::move(manager)),
        _is_camera(config.get_tcp_client_is_camera())
    {

    }

    void TcpClient::Start() {
        if (_is_stopped) {
            _is_stopped = false;
            startConnection(true);
        }
    }

    void TcpClient::Stop() {
        if (!_is_stopped) {
            _is_stopped = true;
            auto self(shared_from_this());
            boost::asio::post(
                net::make_strand(_context),
                [this, s = std::move(self)]() {
                    error_code ec;
                    disconnect(ec);
                }
            );
        }
    }

    void TcpClient::startConnection(const bool is_initial_connection) {
        if (!is_initial_connection) {
            if (_is_stopped) return;
            std::this_thread::sleep_for(5s);
        }
        if (_is_stopped) return;
        if (_socket == nullptr) {
            _socket = std::make_shared<tcp::socket>(net::make_strand(_context));
        }
        std::cout << "TcpClient running async connect" << std::endl;
        auto self(shared_from_this());
        _socket->async_connect(
            _remote_endpoint,
            [this, s = std::move(self)](error_code ec) {
                std::cout << "TcpClient attempting connection" << std::endl;
                std::cout << ec << std::endl;
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
        std::cout << "TcpClient connected; waiting to write" << std::endl;
        _manager->CreateCameraClientConnection();
    }

    void TcpClient::Post(std::shared_ptr<SizedBuffer> &&buffer) {
        if (_is_stopped || !_is_connected) return;
        auto buffer_memory = buffer->GetMemory();
        auto buffer_size = buffer->GetSize();
        std::string str((char *)buffer_memory);
        auto self(shared_from_this());
        _socket->async_send(
            net::buffer(buffer_memory, buffer_size),
            [this, s = std::move(self), send_buffer = std::move(buffer)](error_code ec, std::size_t bytes_written) {
                if (ec || bytes_written != send_buffer->GetSize()) {
                    reconnect(ec);
                }
            }
        );
    }

    void TcpClient::startRead() {
        std::cout << "TcpClient connected; starting to read" << std::endl;
        _buffer_pool = _manager->CreateHeadsetClientConnection();
        auto self(shared_from_this());
        net::dispatch(
            _socket->get_executor(),
            [this, s = std::move(self)]() {
                readHeader();
            }
        );
    }

    void TcpClient::readHeader() {
        if (_is_stopped || !_is_connected) return;
        auto self(shared_from_this());
        _socket->async_receive(
            net::buffer(_header.Data(), TcpReaderMessage::header_length),
            [this, s = std::move(self)] (error_code ec, std::size_t bytes_written) mutable {
                if (!ec && bytes_written == TcpReaderMessage::header_length && _header.DecodeHeader() && !_is_stopped) {
                    readBody();
                } else {
                    reconnect(ec);
                }
            }
        );
    }

    void TcpClient::readBody() {
        if (_is_stopped || !_is_connected) return;
        auto buffer = _buffer_pool->GetResizableBuffer();
        auto buffer_memory = buffer->GetMemory();
        auto self(shared_from_this());
        _socket->async_receive(
            net::buffer(buffer_memory, _header.BodyLength()),
            [this, s = std::move(self), camera_buffer = std::move(buffer)] (error_code ec, std::size_t bytes_written) mutable {
                if (!ec && bytes_written == _header.BodyLength() && !_is_stopped) {
                    camera_buffer->SetSize(bytes_written);
                    _buffer_pool->PostResizableBuffer(std::move(camera_buffer));
                    readHeader();
                } else {
                    reconnect(ec);
                }
            }
        );

    }

    void TcpClient::reconnect(error_code ec) {
        std::cout << "Socket has error " << ec << "; attempting to reconnect" << std::endl;
        disconnect(ec);
        startConnection(false);
    }

    void TcpClient::disconnect(error_code ec) {
        _is_connected = false;
        if (_socket->is_open()) {
            _socket->shutdown(tcp::socket::shutdown_both, ec);
            _socket->close();
            _socket.reset();
            _socket = nullptr;
        }
        if (_is_camera) {
            _manager->DestroyCameraClientConnection();
        } else {
            _manager->DestroyHeadsetClientConnection();
        }
    }
}
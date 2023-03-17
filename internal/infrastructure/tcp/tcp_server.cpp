//
// Created by brucegoose on 3/5/23.
//

#include "tcp_server.hpp"

namespace infrastructure {

    void fail(error_code ec, char const* what) {
        std::cerr << what << ": " << ec.message() << "\n";
    }

    void failOut(error_code ec, char const* what) {
        std::cerr << what << ": " << ec.message() << "\n";
        throw std::runtime_error(ec.message());
    }

    TcpServer::TcpServer(const TcpServerConfig &config, net::io_context &context, std::shared_ptr<TcpServerManager> &manager):
        _context(context),
        _endpoint(tcp::v4(), config.get_tcp_server_port()),
        _acceptor(net::make_strand(context)),
        _manager(manager)
    {
        error_code ec;

        // Open the acceptor
        _acceptor.open(_endpoint.protocol(), ec);
        if(ec)
        {
            failOut(ec, "open");
        }

        // Allow address reuse
        _acceptor.set_option(net::socket_base::reuse_address(true), ec);
        if(ec)
        {
            failOut(ec, "set_option");
        }

        // Bind to the server address
        _acceptor.bind(_endpoint, ec);
        if(ec)
        {
            failOut(ec, "bind");
        }

        // Start listening for connections
        _acceptor.listen(net::socket_base::max_listen_connections, ec);
        if(ec)
        {
            failOut(ec, "listen");
        }
    }

    void TcpServer::Start() {
        if (_is_stopped) {
            _is_stopped = false;
            acceptConnections();
        }

    }

    void TcpServer::Stop() {
        if (!_is_stopped) {
            _is_stopped = true;
            boost::asio::post(
                net::make_strand(_context),
                [&acceptor = _acceptor]() { acceptor.cancel(); }
            );
        }
    }

    void TcpServer::acceptConnections() {
        _acceptor.async_accept(
            net::make_strand(_context),
            [this](error_code ec, tcp::socket socket) {
                if (_is_stopped) {
                    return;
                }
                if (!ec) {
                    socket.set_option(tcp::no_delay(true));
                    net::socket_base::keep_alive option(true);
                    socket.set_option(option);
                    auto connection_type = _manager->GetConnectionType(socket.remote_endpoint());
                    if (connection_type == TcpConnectionType::CAMERA_CONNECTION) {
                        std::shared_ptr<TcpCameraSession>(new TcpCameraSession(std::move(socket), _manager))->Run();
                    }
                }
                if (ec != net::error::operation_aborted) {
                    acceptConnections();
                }
            }
        );
    }

    TcpCameraSession::TcpCameraSession(tcp::socket &&socket, std::shared_ptr<TcpServerManager> &manager):
        _socket(std::move(socket)), _manager(manager)
    {
        auto [session_id, buffer_pool] = _manager->CreateCameraConnection(_socket.remote_endpoint());
        _session_id = session_id;
        _buffer_pool = buffer_pool;
    }

    void TcpCameraSession::Run() {
        auto self(shared_from_this());
        net::dispatch(
            _socket.get_executor(),
            [this, self]() {
                readStream();
            }
        );
    }

    void TcpCameraSession::readStream() {
        auto buffer = _buffer_pool->GetSizedBuffer();
        auto buffer_memory = buffer->GetMemory();
        auto buffer_size = buffer->GetSize();
        auto self(shared_from_this());
        _socket.async_receive(
            boost::asio::buffer(buffer_memory, buffer_size),
            [this, self, camera_buffer = std::move(buffer)] (error_code ec, std::size_t bytes_written) mutable {
                if (!ec && bytes_written == camera_buffer->GetSize()) {
                    _buffer_pool->SendSizedBuffer(std::move(camera_buffer));
                    readStream();
                } else {
                    TryClose();
                }
            }
        );
    }

    void TcpCameraSession::TryClose() {
        if (_socket.is_open()) {
            error_code ec;
            _socket.shutdown(tcp::socket::shutdown_send, ec);
        }
        _manager->DestroyCameraConnection(
            _socket.remote_endpoint(), _session_id
        );
    }

    TcpHeadsetSession::TcpHeadsetSession(tcp::socket &&socket, std::shared_ptr<TcpServerManager> &manager):
        _socket(std::move(socket)),
        _is_live(true),
        _manager(manager)
    {
        auto self(shared_from_this());
        _session_id = _manager->CreateHeadsetConnection(
            _socket.remote_endpoint(),
            [this, self](std::shared_ptr<SizedBuffer> &&buffer) {
                write(std::move(buffer));
            }
        );
    }

    void TcpHeadsetSession::write(std::shared_ptr<SizedBuffer> &&buffer) {
        if (!_is_live) {
            return;
        }
        auto buffer_memory = buffer->GetMemory();
        auto buffer_size = buffer->GetSize();
        auto self(shared_from_this());
        _socket.async_send(
            net::buffer(buffer_memory, buffer_size),
            [this, self, send_buffer = std::move(buffer)](error_code ec, std::size_t bytes_written) mutable {
                if (ec || bytes_written != send_buffer->GetSize()) {
                    TryClose();
                }
            }
        );
    }


    void TcpHeadsetSession::TryClose() {
        _is_live = false;
        if (_socket.is_open()) {
            error_code ec;
            _socket.shutdown(tcp::socket::shutdown_send, ec);
        }
        _manager->DestroyHeadsetConnection(_socket.remote_endpoint(), _session_id);
    }
}
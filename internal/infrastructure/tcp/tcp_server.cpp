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

    TcpServer::TcpServer(const TcpServerConfig &config, net::io_context &context, std::shared_ptr<TcpServerManager> manager):
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
            auto self(shared_from_this());
            boost::asio::post(
                net::make_strand(_context),
                [&acceptor = _acceptor, s = std::move(self)]() {
                    acceptor.cancel();
                }
            );
        }
    }

    void TcpServer::acceptConnections() {
        std::cout << "TcpServer: starting to accept connections" << std::endl;
        auto self(shared_from_this());
        _acceptor.async_accept(
            net::make_strand(_context),
            [this, s = std::move(self)](error_code ec, tcp::socket socket) {
                std::cout << "TcpServer: attempting connection" << std::endl;
                std::cout << ec << std::endl;
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
                    } else if (connection_type == TcpConnectionType::HEADSET_CONNECTION) {
                        std::shared_ptr<TcpHeadsetSession>(
                                new TcpHeadsetSession(std::move(socket), _manager)
                        )->ConnectAndWait();
                    } else {
                        std::cout << "TcpServer: Unknown connection, abort" << std::endl;
                        socket.shutdown(tcp::socket::shutdown_send, ec);
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
    {}

    void TcpCameraSession::Run() {
        std::cout << "TcpCameraSession: creating connection" << std::endl;
        auto self(shared_from_this());
        auto [session_id, plane_buffer_pool] = _manager->CreateCameraServerConnection(self);
        _session_id = session_id;
        _plane_buffer_pool = plane_buffer_pool;

        std::cout << "TcpCameraSession: running readStream" << std::endl;
        net::dispatch(
            _socket.get_executor(),
            [this, s = std::move(self)]() {
                readStream();
            }
        );
    }

    void TcpCameraSession::readStream() {
        auto plane_buffer = _plane_buffer_pool->GetSizedBufferPool();
        auto plane = plane_buffer->GetSizedBuffer();
        auto self(shared_from_this());
        _socket.async_receive(
            boost::asio::buffer(plane->GetMemory(), plane->GetSize()),
            [this, s = std::move(self), camera_pool = std::move(plane_buffer), camera_buffer = std::move(plane)]
            (error_code ec, std::size_t bytes_written) mutable {
                if (!ec && bytes_written == camera_buffer->GetSize()) {
                    auto next_plane = camera_pool->GetSizedBuffer();
                    if (next_plane) {
                        readStreamPlane(std::move(camera_pool), std::move(next_plane));
                    } else {
                        _plane_buffer_pool->PostSizedBufferPool(std::move(camera_pool));
                        readStream();
                    }
                } else if (!ec) {
                    continueReadPlane(std::move(camera_pool), std::move(camera_buffer), bytes_written);
                } else {
                    TryClose();
                }
            }
        );
    }

    void TcpCameraSession::readStreamPlane(
        std::shared_ptr<SizedBufferPool> &&pool, std::shared_ptr<SizedBuffer> &&plane
    ) {
        auto self(shared_from_this());
        _socket.async_receive(
            boost::asio::buffer(plane->GetMemory(), plane->GetSize()),
            [this, s = std::move(self), camera_pool = std::move(pool), camera_buffer = std::move(plane)]
            (error_code ec, std::size_t bytes_written) mutable {
                if (!ec && bytes_written == camera_buffer->GetSize()) {
                    auto next_plane = camera_pool->GetSizedBuffer();
                    if (next_plane) {
                        readStreamPlane(std::move(camera_pool), std::move(next_plane));
                    } else {
                        _plane_buffer_pool->PostSizedBufferPool(std::move(camera_pool));
                        readStream();
                    }
                } else if (!ec) {
                    continueReadPlane(std::move(camera_pool), std::move(camera_buffer), bytes_written);
                } else {
                    TryClose();
                }
            }
        );
    }

    void TcpCameraSession::continueReadPlane(
        std::shared_ptr<SizedBufferPool> &&pool, std::shared_ptr<SizedBuffer> &&buffer,
        std::size_t bytes_written
    ) {
        auto buffer_memory = static_cast<uint8_t *>(buffer->GetMemory()) + bytes_written;
        auto buffer_size = buffer->GetSize() - bytes_written;
        auto self(shared_from_this());
        _socket.async_receive(
                boost::asio::buffer(buffer_memory, buffer_size),
                [
                    this, s = std::move(self), camera_pool = std::move(pool),
                    camera_buffer = std::move(buffer), current_bytes = bytes_written
                ]
                (error_code ec, std::size_t bytes_written) mutable {
                    if (!ec && (bytes_written + current_bytes) == camera_buffer->GetSize()) {
                        auto next_plane = camera_pool->GetSizedBuffer();
                        if (next_plane) {
                            readStreamPlane(std::move(camera_pool), std::move(next_plane));
                        } else {
                            _plane_buffer_pool->PostSizedBufferPool(std::move(camera_pool));
                            readStream();
                        }
                    } else if (!ec) {
                        continueReadPlane(
                            std::move(camera_pool), std::move(camera_buffer), bytes_written + current_bytes
                        );
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
        auto self(shared_from_this());
        _manager->DestroyCameraServerConnection(self);
    }

    TcpHeadsetSession::TcpHeadsetSession(tcp::socket &&socket, std::shared_ptr<TcpServerManager> &manager):
        _socket(std::move(socket)),
        _is_live(true),
        _manager(manager)
    {
        std::cout << "TcpHeadsetSession: creating connection" << std::endl;
    }

    void TcpHeadsetSession::ConnectAndWait() {
        auto self(shared_from_this());
        _session_id = _manager->CreateHeadsetServerConnection(self);
    }

    void TcpHeadsetSession::Write(std::shared_ptr<SizedBuffer> &&buffer) {
        if (!_is_live) {
            return;
        }
        auto buffer_memory = buffer->GetMemory();
        auto buffer_size = buffer->GetSize();
        auto self(shared_from_this());
        _socket.async_send(
            net::buffer(buffer_memory, buffer_size),
            [this, s = std::move(self), send_buffer = std::move(buffer)](error_code ec, std::size_t bytes_written) mutable {
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
        auto self(shared_from_this());
        _manager->DestroyHeadsetServerConnection(self);
    }
}
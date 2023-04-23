//
// Created by brucegoose on 3/5/23.
//

#include "tcp_server.hpp"

namespace infrastructure {

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

        std::cout << "TcpCameraSession: running read" << std::endl;
        net::dispatch(
            _socket.get_executor(),
            [this, s = std::move(self)]() {
                readHeader();
            }
        );
    }

    void TcpCameraSession::readHeader() {
        auto self(shared_from_this());
        _header.ResetHeader();
        _socket.async_receive(
                net::buffer(_header.Data(), _header.Size()),
                [this, s = std::move(self)] (error_code ec, std::size_t bytes_written) mutable {
                    if (!ec && bytes_written == _header.Size() && _header.Ok()) {
                        readBody();
                        return;
                    }
                    std::cout << "TcpCameraSession: error reading header: ";
                    if (ec) {
                        std::cout << ec;
                    } else if (bytes_written != _header.Size()) {
                        std::cout << bytes_written << " != " << _header.Size();
                    } else if (_header.Ok()) {
                        std::cout << "unable to parse header";
                    }
                    std::cout << "; closing" << std::endl;
                    TryClose(true);
                }
        );
    }

    void TcpCameraSession::readBody() {
        if (_plane_buffer == nullptr) {
            _plane_buffer = _plane_buffer_pool->GetSizedBufferPool();
        }
        if (_buffer == nullptr) {
            _buffer = _plane_buffer->GetSizedBuffer();
        }
        auto self(shared_from_this());
        _socket.async_receive(
            boost::asio::buffer((uint8_t *) _buffer->GetMemory() + _header.BytesWritten(), _header.DataLength()),
            [this, s = std::move(self)] (error_code ec, std::size_t bytes_written) mutable {
                if (ec) {
                    std::cout << "TcpCameraSession: error reading body: " << ec << "; closing" << std::endl;
                    TryClose(true);
                } else if (bytes_written != _header.DataLength()) {
                    _header.OffsetPacket(bytes_written);
                    readBody();
                }
                else if (_header.IsFinished()) {
                    _buffer = _plane_buffer->GetSizedBuffer();
                    if (_buffer == nullptr) {
                        _plane_buffer_pool->PostSizedBufferPool(std::move(_plane_buffer));
                        _plane_buffer = nullptr;
                    }
                    readHeader();
                }
            }
        );
    }

    void TcpCameraSession::TryClose(bool is_self_close) {
        if (_socket.is_open()) {
            error_code ec;
            _socket.shutdown(tcp::socket::shutdown_send, ec);
        }
        if (is_self_close) {
            auto self(shared_from_this());
            _manager->DestroyCameraServerConnection(self);
        }
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
        bool write_in_progress = false;
        {
            std::unique_lock<std::mutex> lock(_message_mutex);
            write_in_progress = !_message_queue.empty();
            _message_queue.push(std::move(buffer));
        }
        if (!write_in_progress) {
            _header.SetupHeader(_message_queue.front()->GetSize());
            writeHeader();
        }
    }

    void TcpHeadsetSession::writeHeader() {
        auto self(shared_from_this());
        _socket.async_send(
                net::buffer(_header.Data(), _header.Size()),
                [this, s = std::move(self)](error_code ec, std::size_t bytes_written) mutable {
                    if (ec || bytes_written != _header.Size()) {
                        TryClose(true);
                    } else {
                        writeBody();
                    }
                }
        );
    }

    void TcpHeadsetSession::writeBody() {
        auto &buffer = _message_queue.front();
        auto self(shared_from_this());
        _socket.async_send(
                net::buffer((uint8_t *) buffer->GetMemory() + _header.BytesWritten(), _header.DataLength()),
                [this, s = std::move(self)](error_code ec, std::size_t bytes_written) mutable {
                    if (ec || bytes_written != _header.DataLength()) {
                        TryClose(true);
                        return;
                    }
                    if (_header.IsFinished()) {
                        bool messages_remaining = false;
                        {
                            std::unique_lock<std::mutex> lock(_message_mutex);
                            _message_queue.pop();
                            messages_remaining = !_message_queue.empty();
                        }
                        if (!messages_remaining) {
                            return;
                        }
                        _header.SetupHeader(_message_queue.front()->GetSize());
                    } else {
                        _header.SetupNextHeader();
                    }
                    writeHeader();
                }
        );
    }


    void TcpHeadsetSession::TryClose(bool is_self_close) {
        _is_live = false;
        if (_socket.is_open()) {
            error_code ec;
            _socket.shutdown(tcp::socket::shutdown_send, ec);
        }
        {
            std::unique_lock<std::mutex> lock(_message_mutex);
            while(!_message_queue.empty()) {
                _message_queue.pop();
            }
        }
        if (is_self_close) {
            auto self(shared_from_this());
            _manager->DestroyHeadsetServerConnection(self);
        }
    }
}
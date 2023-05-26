//
// Created by brucegoose on 3/5/23.
//

#include "tcp_server.hpp"

#include <utility>

namespace infrastructure {

    void failOut(error_code ec, char const* what) {
        std::cerr << what << ": " << ec.message() << "\n";
        throw std::runtime_error(ec.message());
    }

    TcpServer::TcpServer(const TcpServerConfig &config, net::io_context &context, std::shared_ptr<TcpServerManager> manager):
        _context(context),
        _endpoint(tcp::v4(), config.get_tcp_server_port()),
        _acceptor(net::make_strand(context)),
        _manager(std::move(manager)),
        _read_timeout(config.get_tcp_server_timeout_on_read()),
        _tcp_camera_session_buffer_count(config.get_tcp_camera_session_buffer_count()),
        _tcp_camera_session_buffer_size(config.get_tcp_server_buffer_size())
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

        _acceptor.set_option(reuse_port(true), ec);
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
            net::post(
                _acceptor.get_executor(),
                [this, self]() {
                    _acceptor.cancel();
                }
            );
        }
    }

    void TcpServer::acceptConnections() {
        std::cout << "TcpServer: starting to accept connections" << std::endl;

        // make sure each session is created with a new strand
        auto socket_ptr = std::make_shared<tcp::socket>(net::make_strand(_context));

        auto self(shared_from_this());
        _acceptor.async_accept(
            *socket_ptr,
            [this, self, socket_ptr](error_code ec) {
                std::cout << "TcpServer: attempting connection" << std::endl;
                if (_is_stopped) {
                    return;
                }
                if (!ec) {
                    socket_ptr->set_option(tcp::no_delay(true));
                    net::socket_base::keep_alive option(true);
                    socket_ptr->set_option(option);
                    socket_ptr->set_option(reuse_port(true));
                    socket_ptr->set_option(tcp::socket::reuse_address(true));
                    const auto remote = socket_ptr->remote_endpoint();
                    auto connection_type = _manager->GetConnectionType(remote);
                    const auto addr = remote.address().to_v4();
                    if (connection_type == TcpConnectionType::CAMERA_CONNECTION) {
                        std::shared_ptr<TcpCameraSession>(
                            new TcpCameraSession(
                                std::move(*socket_ptr), _manager, addr, _read_timeout, _tcp_camera_session_buffer_count,
                                _tcp_camera_session_buffer_size
                            )
                        )->Run();
                    } else if (connection_type == TcpConnectionType::HEADSET_CONNECTION) {
                        /*
                         * TODO: this should have < camera_session_buffers so we never hog a whole sessions stream
                         */
                        std::shared_ptr<TcpHeadsetSession>(
                            new TcpHeadsetSession(
                                std::move(*socket_ptr), _manager, addr, _tcp_camera_session_buffer_count,
                                _tcp_camera_session_buffer_size
                            )
                        )->ConnectAndWait();
                    } else {
                        std::cout << "TcpServer: Unknown connection, abort" << std::endl;
                        socket_ptr->shutdown(tcp::socket::shutdown_both, ec);
                    }
                }
                if (ec != net::error::operation_aborted) {
                    acceptConnections();
                }
            }
        );
    }

    TcpCameraSession::TcpCameraSession(
            tcp::socket &&socket, std::shared_ptr<TcpServerManager> &manager,
            const tcp_addr addr, const int read_timeout, const int buffer_count,
            const int buffer_size
    ):
        _socket(std::move(socket)), _manager(manager), _addr(std::move(addr)),
        _read_timer(socket.get_executor()), _read_timeout(read_timeout)
    {
        _receive_buffer_pool = TcpReadBufferPool::Create(buffer_count, buffer_size);
    }

    void TcpCameraSession::Run() {
        std::cout << "TcpCameraSession: creating connection" << std::endl;
        auto self(shared_from_this());
        _session_id = _manager->CreateCameraServerConnection(std::move(self));

        std::cout << "TcpCameraSession: running read" << std::endl;
        net::dispatch(
            _socket.get_executor(),
            [this, self]() {
                readHeader(0);
            }
        );
    }

    void TcpCameraSession::startTimer() {
        _read_timer.expires_from_now(boost::posix_time::seconds(_read_timeout));
        auto self(shared_from_this());
        _read_timer.async_wait([this, self](error_code ec) {
            if (!ec) {
                TryClose(true);
            }
        });
    }

    void TcpCameraSession::readHeader(std::size_t last_bytes) {
        startTimer();
        auto self(shared_from_this());
        _socket.async_receive(
                net::buffer(_header.Data() + last_bytes, _header.Size() - last_bytes),
                [this, self, last_bytes] (error_code ec, std::size_t bytes_written) mutable {
                    if (ec ==  boost::asio::error::operation_aborted) {
                        std::cout << "TcpCameraSession: readHeader aborted" << std::endl;
                        return;
                    }
                    _read_timer.cancel();
                    auto total_bytes = last_bytes + bytes_written;
                    if (!ec) {
                        if (total_bytes == _header.Size() && _header.Ok()) {
                            readBody();
                            return;
                        } else if (total_bytes != _header.Size()) {
                            readHeader(total_bytes);
                            return;
                        }
                    }
                    std::cout << "TcpCameraSession: error reading header: ";
                    if (ec) {
                        std::cout << ec;
                    } else if (_header.Ok()) {
                        std::cout << "unable to parse header";
                    } else {
                        std::cout << "unknown error";
                    }
                    std::cout << "; closing" << std::endl;
                    TryClose(true);
                }
        );
    }

    void TcpCameraSession::readBody() {
        if (_receive_buffer == nullptr) {
            _receive_buffer = _receive_buffer_pool->GetReadBuffer();
        }

        startTimer();
        auto self(shared_from_this());
        _socket.async_receive(
            boost::asio::buffer((uint8_t *) _receive_buffer->GetMemory() + _header.BytesWritten(), _header.DataLength()),
            [this, self] (error_code ec, std::size_t bytes_written) mutable {
                if (ec ==  boost::asio::error::operation_aborted) {
                    std::cout << "TcpCameraSession: readBody aborted" << std::endl;
                    return;
                }
                _read_timer.cancel();
                if (ec) {
                    std::cout << "TcpCameraSession: error reading body: " << ec << "; closing" << std::endl;
                    TryClose(true);
                    return;
                } else if (bytes_written != _header.DataLength()) {
                    _header.OffsetPacket(bytes_written);
                    readBody();
                    return;
                }
                if (_header.IsFinished()) {
                    if (!_receive_buffer->IsLeakyBuffer()) {
                        _receive_buffer->SetSize(_header.BytesWritten());
                        _manager->PostCameraServerBuffer(_addr, std::move(_receive_buffer));
                    }
                    _receive_buffer = nullptr;
                    _header.ResetHeader();
                }
                readHeader(0);
            }
        );
    }

    void TcpCameraSession::TryClose(bool internal_close) {

        // this might should be done async in a strand
        if (_socket.is_open()) {
            error_code ec;
            _socket.shutdown(tcp::socket::shutdown_both, ec);
            _socket.cancel();
            _socket.release();
        }
        if (_receive_buffer_pool) {
            _receive_buffer_pool.reset();
        }
        if (internal_close) {
            auto self(shared_from_this());
            _manager->DestroyCameraServerConnection(std::move(self));
        }
    }

    TcpCameraSession::~TcpCameraSession() {
        std::cout << "TcpCameraSession: Deconstructed" << std::endl;
    }

    TcpHeadsetSession::TcpHeadsetSession(
        tcp::socket &&socket, std::shared_ptr<TcpServerManager> &manager, tcp_addr addr, const int buffer_count,
        const int buffer_size
    ):
        _socket(std::move(socket)),
        _is_live(true),
        _manager(manager),
        _addr(std::move(addr))
    {
        std::cout << "TcpHeadsetSession: creating connection" << std::endl;
        _copy_buffer_pool = TcpWriteBufferPool::Create(buffer_count, buffer_size);
    }

    void TcpHeadsetSession::ConnectAndWait() {
        auto self(shared_from_this());
        _session_id = _manager->CreateHeadsetServerConnection(std::move(self));
    }

    void TcpHeadsetSession::Write(std::shared_ptr<SizedBuffer> &&buffer) {
        // just post to the executor for synchronization
        auto self(shared_from_this());
        net::post(
            _socket.get_executor(),
            [this, self, copy_buffer = std::move(buffer)]() mutable {
                if (!_is_live) {
                    return;
                }
                auto out_buffer = _copy_buffer_pool->CopyToWriteBuffer(std::move(copy_buffer));
                if (out_buffer == nullptr) {
                    std::cout << "we aren't returning buffers, are we?" << std::endl;
                    TryClose(true);
                    return;
                }
                // TODO: this is overkill; but I'm not confident I can remove it.
                bool write_in_progress = false;
                {
                    std::unique_lock<std::mutex> lock(_message_mutex);
                    write_in_progress = !_message_queue.empty();
                    _message_queue.push(std::move(out_buffer));
                }
                if (!write_in_progress) {
                    _header.SetupHeader(_message_queue.front()->GetSize());
                    writeHeader(0);
                }
            }
        );
    }

    void TcpHeadsetSession::writeHeader(std::size_t last_bytes) {
        auto self(shared_from_this());
        _socket.async_send(
            net::buffer(_header.Data() + last_bytes, _header.Size() - last_bytes),
            [this, self, last_bytes](error_code ec, std::size_t bytes_written) mutable {
                auto total_bytes = last_bytes + bytes_written;
                if (!ec) {
                    if (total_bytes == _header.Size()) {
                        writeBody();
                        return;
                    } else if (total_bytes < _header.Size()) {
                        writeHeader(total_bytes);
                        return;
                    }
                }
                std::cout << "TcpHeadsetSession: error writing header: ";
                if (ec) {
                    std::cout << ec;
                } else {
                    std::cout << "unknown error";
                }
                std::cout << "; closing" << std::endl;
                TryClose(true);
            }
        );
    }

    void TcpHeadsetSession::writeBody() {
        auto &buffer = _message_queue.front();
        auto self(shared_from_this());
        _socket.async_send(
                net::buffer((uint8_t *) buffer->GetMemory() + _header.BytesWritten(), _header.DataLength()),
                [this, self](error_code ec, std::size_t bytes_written) mutable {
                    if (ec) {
                        std::cout << "TcpHeadsetSession: error writing body: " << ec << "; disconnecting" << std::endl;
                        TryClose(true);
                        return;
                    } else if (bytes_written != _header.DataLength()) {
                        _header.OffsetPacket(bytes_written);
                        writeBody();
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
                    writeHeader(0);
                }
        );
    }


    void TcpHeadsetSession::TryClose(const bool internal_close) {
        _is_live = false;
        // this might should be done async in a strand
        if (_socket.is_open()) {
            error_code ec;
            _socket.shutdown(tcp::socket::shutdown_both, ec);
        }
        {
            std::unique_lock<std::mutex> lock(_message_mutex);
            while(!_message_queue.empty()) {
                _message_queue.pop();
            }
        }
        if (internal_close) {
            auto self(shared_from_this());
            _manager->DestroyHeadsetServerConnection(std::move(self));
        }
    }

    TcpHeadsetSession::~TcpHeadsetSession() {
        std::cout << "TcpHeadsetSession: Deconstructed" << std::endl;
    }
}
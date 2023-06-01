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
        _remote_endpoint(
            net::ip::address::from_string(config.get_tcp_server_host()),
            config.get_tcp_server_port()
        ),
        _manager(std::move(manager)),
        _use_fixed_port(config.get_tcp_client_used_fixed_port()),
        _connection_type(config.get_tcp_client_connection_type()),
        _read_timer(net::make_strand(context)),
        _read_timeout(config.get_tcp_client_timeout_on_read())
    {
        if (_connection_type == ConnectionType::UNKNOWN_CONNECTION) {
            throw std::runtime_error("TcpClient::TcpClient INVALID CONNECTION TYPE");
        }
        if (_connection_type != ConnectionType::CAMERA_CONNECTION) {
            _receive_buffer_pool = TcpReadBufferPool::Create(
                config.get_tcp_client_read_buffer_count(),
                config.get_tcp_client_read_buffer_size()
            );
        }
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
            std::promise<void> done_promise;
            auto done_future = done_promise.get_future();
            auto self(shared_from_this());
            net::post(
                _read_timer.get_executor(),
                [this, self, p = std::move(done_promise)]() mutable {
                    error_code ec;
                    disconnect(ec);
                    p.set_value();
                }
            );
            done_future.wait();
        }
    }
    TcpClient::~TcpClient() {
        std::cout << "TcpClient Deconstructed" << std::endl;
    }

    void TcpClient::startConnection(const bool is_initial_connection) {
        if (!is_initial_connection) {
            if (_is_stopped) return;
            std::this_thread::sleep_for(1s);
        }
        if (_is_stopped) return;
        if (_socket == nullptr) {
            if (_use_fixed_port) {
                uint32_t port = 0;
                switch (_connection_type) {
                    case ConnectionType::CAMERA_CONNECTION:
                        port = 11111;
                        break;
                    case ConnectionType::HEADSET_CONNECTION:
                        port = 21222;
                        break;
                    case ConnectionType::DISPLAY_CONNECTION:
                        port = 31333;
                        break;
                }
                std::cout << port << std::endl;
                auto endpoint = tcp::endpoint(tcp::v4(), port);
                _socket = std::make_shared<tcp::socket>(_read_timer.get_executor(), endpoint);
            } else {
                _socket = std::make_shared<tcp::socket>(_read_timer.get_executor());
            }

        }
        _header.ResetHeader();
        std::cout << "TcpClient running async connect" << std::endl;
        auto self(shared_from_this());
        _socket->async_connect(
            _remote_endpoint,
            [this, self](error_code ec) {
                std::cout << "TcpClient attempting connection" << std::endl;
                if (!ec && !_is_stopped) {
                    _socket->set_option(tcp::no_delay(true));
                    net::socket_base::keep_alive option(true);
                    _socket->set_option(option);
                    _socket->set_option(reuse_port(true));
                    _socket->set_option(tcp::socket::reuse_address(true));
                    _is_connected = true;
                    if (_connection_type == ConnectionType::CAMERA_CONNECTION) {
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
        bool write_in_progress = false;
        {
            std::unique_lock<std::mutex> lock(_send_buffer_mutex);
            write_in_progress = !_send_buffer_queue.empty();
            _send_buffer_queue.push(std::move(buffer));
        }
        if (!write_in_progress) {
            _header.SetupHeader(_send_buffer_queue.front()->GetSize());
            writeHeader(0);
        }
    }

    void TcpClient::writeHeader(std::size_t last_bytes) {
        if (_is_stopped || !_is_connected) return;
        auto self(shared_from_this());
        _socket->async_send(
            net::buffer(_header.Data() + last_bytes, _header.Size() - last_bytes),
            [this, self, last_bytes](error_code ec, std::size_t bytes_written) mutable {
                if (_is_stopped || !_is_connected) return;
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
                std::cout << "TcpClient: error writing header: ";
                if (ec) {
                    std::cout << ec;
                } else {
                    std::cout << "unknown error";
                }
                std::cout << "; reconnecting" << std::endl;
                reconnect(ec);
            }
        );

    }

    void TcpClient::writeBody() {
        if (_is_stopped || !_is_connected) return;
        auto &buffer = _send_buffer_queue.front();
        auto self(shared_from_this());
        _socket->async_send(
            net::buffer((uint8_t *) buffer->GetMemory() + _header.BytesWritten(), _header.DataLength()),
            [this, self](error_code ec, std::size_t bytes_written) mutable {
                if (_is_stopped || !_is_connected) return;
                if (ec) {
                    std::cout << "TcpClient: error writing body: " << ec << "; reconnecting" << std::endl;
                    reconnect(ec);
                    return;
                } else if (bytes_written != _header.DataLength()) {
                    _header.OffsetPacket(bytes_written);
                    writeBody();
                    return;
                }
                if (_header.IsFinished()) {
                    bool messages_remaining = false;
                    {
                        std::unique_lock<std::mutex> lock(_send_buffer_mutex);
                        _send_buffer_queue.pop();
                        messages_remaining = !_send_buffer_queue.empty();
                    }
                    if (!messages_remaining) {
                        return;
                    }
                    _header.SetupHeader(_send_buffer_queue.front()->GetSize());
                } else {
                    _header.SetupNextHeader();
                }
                writeHeader(0);
            }
        );
    }

    void TcpClient::startRead() {
        std::cout << "TcpClient connected; starting to read" << std::endl;
        _manager->CreateHeadsetClientConnection();
        auto self(shared_from_this());
        net::dispatch(
            _socket->get_executor(),
            [this, self]() {
                if (_is_stopped || !_is_connected) return;
                readHeader(0);
            }
        );
    }

    void TcpClient::startTimer() {
        _read_timer.expires_from_now(boost::posix_time::seconds(_read_timeout));
        auto self(shared_from_this());
        _read_timer.async_wait([this, self](error_code ec) {
            if (!ec) {
                reconnect(ec);
            }
        });
    }

    void TcpClient::readHeader(std::size_t last_bytes) {
        if (_is_stopped || !_is_connected) return;
        startTimer();
        auto self(shared_from_this());
        _socket->async_receive(
            net::buffer(_header.Data() + last_bytes, _header.Size() - last_bytes),
            [this, self, last_bytes] (error_code ec, std::size_t bytes_written) mutable {
                if (ec == net::error::operation_aborted) {
                    std::cout << "TcpClient: readHeader aborted" << std::endl;
                    return;
                }
                _read_timer.cancel();
                if (_is_stopped || !_is_connected) return;

                auto total_bytes = last_bytes + bytes_written;
                if (!ec && total_bytes == _header.Size() && _header.Ok()) {
                    readBody();
                    return;
                } else if (!ec && total_bytes < _header.Size()) {
                    readHeader(total_bytes);
                    return;
                }
                std::cout << "TcpClient: error reading header: ";
                if (ec) {
                    std::cout << ec;
                } else if (_header.Ok()) {
                    std::cout << "unable to parse header";
                } else {
                    std::cout << "unknown error";
                }
                std::cout << "; reconnecting" << std::endl;
                reconnect(ec);
            }
        );
    }

    void TcpClient::readBody() {
        if (_is_stopped || !_is_connected) return;
        if (_receive_buffer == nullptr) {
            _receive_buffer = _receive_buffer_pool->GetReadBuffer();
        }
        startTimer();
        auto self(shared_from_this());
        _socket->async_receive(
            net::buffer((uint8_t *) _receive_buffer->GetMemory() + _header.BytesWritten(), _header.DataLength()),
            [this, self] (error_code ec, std::size_t bytes_written) mutable {
                if (ec ==  net::error::operation_aborted) {
                    std::cout << "TcpClient: readBody aborted" << std::endl;
                    return;
                }
                _read_timer.cancel();
                if (_is_stopped || !_is_connected) return;
                if (ec) {
                    std::cout << "TcpClient: error reading body: " << ec << "; reconnecting" << std::endl;
                    reconnect(ec);
                    return;
                } else if (bytes_written != _header.DataLength()) {
                    _header.OffsetPacket(bytes_written);
                    readBody();
                    return;
                }
                if (_header.IsFinished()) {
                    if (!_receive_buffer->IsLeakyBuffer()) {
                        _receive_buffer->SetSize(_header.BytesWritten());
                        _manager->PostHeadsetClientBuffer(std::move(_receive_buffer));
                    }
                    _receive_buffer = nullptr;
                    _header.ResetHeader();
                }
                readHeader(0);
            }
        );

    }

    void TcpClient::reconnect(error_code ec) {
        std::cout << "TCP Client: Socket has error " << ec << "; attempting to reconnect" << std::endl;
        disconnect(ec);
        startConnection(false);
    }

    void TcpClient::disconnect(error_code ec) {
        _is_connected = false;
        if (_socket && _socket->is_open()) {
            _socket->shutdown(tcp::socket::shutdown_both, ec);
            _socket.reset();
            _socket = nullptr;
        }
        if (_connection_type == ConnectionType::CAMERA_CONNECTION) {
            {
                std::unique_lock<std::mutex> lock(_send_buffer_mutex);
                while(!_send_buffer_queue.empty()) {
                    _send_buffer_queue.pop();
                }
            }
            _manager->DestroyCameraClientConnection();
        } else {
            _receive_buffer = nullptr;
            _manager->DestroyHeadsetClientConnection();
        }
    }
}
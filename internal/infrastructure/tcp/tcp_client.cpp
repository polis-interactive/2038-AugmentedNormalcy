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
            std::mutex done_mux;
            std::condition_variable done_cv;
            auto self(shared_from_this());
            boost::asio::post(
                net::make_strand(_context),
                [this, s = std::move(self), &done_mux, &done_cv]() {
                    error_code ec;
                    disconnect(ec);
                    std::lock_guard lk(done_mux);
                    done_cv.notify_one();
                }
            );
            std::unique_lock lk(done_mux);
            done_cv.wait(lk);
        }
    }
    TcpClient::~TcpClient() {}

    void TcpClient::startConnection(const bool is_initial_connection) {
        if (!is_initial_connection) {
            if (_is_stopped) return;
            std::this_thread::sleep_for(1s);
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

    void TcpClient::Post(std::shared_ptr<SizedBufferPool> &&buffer) {
        if (_is_stopped || !_is_connected) return;
        bool write_in_progress = false;
        {
            std::unique_lock<std::mutex> lock(_send_plane_buffer_mutex);
            write_in_progress = !_send_plane_buffer_queue.empty();
            _send_plane_buffer_queue.push(std::move(buffer));
        }
        if (!write_in_progress) {
            _send_buffer = _send_plane_buffer_queue.front()->GetSizedBuffer();
            _header.SetupHeader(_send_buffer->GetSize());
            writeHeader();
        }
    }

    void TcpClient::writeHeader() {
        if (_is_stopped || !_is_connected) return;
        auto self(shared_from_this());
        _socket->async_send(
                net::buffer(_header.Data(), _header.Size()),
                [this, s = std::move(self)](error_code ec, std::size_t bytes_written) mutable {
                    if (_is_stopped || !_is_connected) return;
                    if (!ec && bytes_written == _header.Size()) {
                        writeBody();
                        return;
                    }
                    std::cout << "TcpClient: error writing header: ";
                    if (ec) {
                        std::cout << ec;
                    } else if (bytes_written != _header.Size()) {
                        std::cout << bytes_written << " != " << _header.Size();
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
        auto self(shared_from_this());
        _socket->async_send(
            net::buffer((uint8_t *) _send_buffer->GetMemory() + _header.BytesWritten(), _header.DataLength()),
            [this, s = std::move(self)](error_code ec, std::size_t bytes_written) mutable {
                if (_is_stopped || !_is_connected) return;
                if (ec || bytes_written != _header.DataLength()) {
                    reconnect(ec);
                    return;
                }
                if (_header.IsFinished()) {
                    _send_buffer = _send_plane_buffer_queue.front()->GetSizedBuffer();
                    if (_send_buffer == nullptr) {
                        bool messages_remaining = false;
                        {
                            std::unique_lock<std::mutex> lock(_send_plane_buffer_mutex);
                            _send_plane_buffer_queue.pop();
                            messages_remaining = !_send_plane_buffer_queue.empty();
                        }
                        if (!messages_remaining) {
                            return;
                        }
                        _send_buffer = _send_plane_buffer_queue.front()->GetSizedBuffer();
                    }
                    _header.SetupHeader(_send_buffer->GetSize());
                } else {
                    _header.SetupNextHeader();
                }
                writeHeader();
            }
        );
    }

    void TcpClient::startRead() {
        std::cout << "TcpClient connected; starting to read" << std::endl;
        _receive_buffer_pool = _manager->CreateHeadsetClientConnection();
        auto self(shared_from_this());
        net::dispatch(
            _socket->get_executor(),
            [this, s = std::move(self)]() {
                if (_is_stopped || !_is_connected) return;
                readHeader();
            }
        );
    }

    void TcpClient::readHeader() {
        if (_is_stopped || !_is_connected) return;
        auto self(shared_from_this());
        _socket->async_receive(
            net::buffer(_header.Data(), _header.Size()),
            [this, s = std::move(self)] (error_code ec, std::size_t bytes_written) mutable {
                if (_is_stopped || !_is_connected) return;
                if (ec || bytes_written != _header.Size() || !_header.Ok()) {
                    reconnect(ec);
                    return;
                }
                readBody();
            }
        );
    }

    void TcpClient::readBody() {
        if (_is_stopped || !_is_connected) return;
        if (_receive_buffer == nullptr) {
            _receive_buffer = _receive_buffer_pool->GetResizableBuffer();
        }
        auto self(shared_from_this());
        _socket->async_receive(
            net::buffer((uint8_t *) _receive_buffer->GetMemory() + _header.BytesWritten(), _header.DataLength()),
            [this, s = std::move(self)] (error_code ec, std::size_t bytes_written) mutable {
                if (_is_stopped || !_is_connected) return;
                if (ec || bytes_written != _header.DataLength()) {
                    reconnect(ec);
                    return;
                }
                if (_header.IsFinished()) {
                    _receive_buffer->SetSize(_header.BytesWritten());
                    _receive_buffer_pool->PostResizableBuffer(std::move(_receive_buffer));
                    _receive_buffer = nullptr;
                    _header.ResetHeader();
                }
                readHeader();
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
            _socket->close();
            _socket.reset();
            _socket = nullptr;
        }
        if (_is_camera) {
            _send_buffer = nullptr;
            {
                std::unique_lock<std::mutex> lock(_send_plane_buffer_mutex);
                while(!_send_plane_buffer_queue.empty()) {
                    _send_plane_buffer_queue.pop();
                }
            }
            _manager->DestroyCameraClientConnection();
        } else {
            _receive_buffer = nullptr;
            _receive_buffer_pool = nullptr;
            _manager->DestroyHeadsetClientConnection();
        }
    }
}
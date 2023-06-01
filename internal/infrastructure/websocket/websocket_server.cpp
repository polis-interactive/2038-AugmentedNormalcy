//
// Created by brucegoose on 5/29/23.
//

#include "websocket_server.hpp"

namespace infrastructure {

    WebsocketServer::WebsocketServer(
        const infrastructure::WebsocketServerConfig &config, net::io_context &context,
        std::shared_ptr<WebsocketServerManager> manager
    ):
        _context(context),
        _endpoint(tcp::v4(), config.get_websocket_server_port()),
        _acceptor(net::make_strand(context)),
        _manager(std::move(manager)),
        _read_write_timeout(config.get_websocket_server_timeout())
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

    void WebsocketServer::Start() {
        if (_is_stopped) {
            _is_stopped = false;
            acceptConnections();
        }
    }

    void WebsocketServer::acceptConnections() {

        if (_is_stopped) return;

        std::cout << "WebsocketServer: starting to accept connections" << std::endl;

        _acceptor.async_accept(
            net::make_strand(_context),
            beast::bind_front_handler(
                &WebsocketServer::onAccept,
                shared_from_this()
            )
        );
    }

    void WebsocketServer::onAccept(beast::error_code ec, tcp::socket socket) {

        if (_is_stopped) return;

        std::cout << "WebsocketServer: attempting connection" << std::endl;

        if (!ec) {

            const auto remote = socket.remote_endpoint();
            auto connection_type = _manager->GetConnectionType(remote);
            const auto addr = remote.address().to_v4();

            if (connection_type == ConnectionType::UNKNOWN_CONNECTION) {
                std::cout << "WebsocketServer: Unknown connection, abort" << std::endl;
                socket.shutdown(tcp::socket::shutdown_both, ec);
            } else {
                handleConnection(connection_type == ConnectionType::CAMERA_CONNECTION, addr, std::move(socket));
            }

        } else {
            std::cout << "WebsocketServer::onAccept received err " << ec << " while connecting" << std::endl;
        }

        acceptConnections();
    }

    void WebsocketServer::handleConnection(const bool is_camera, const tcp_addr &addr, tcp::socket &&socket) {
        auto session = std::make_shared<WebsocketSession>(
              is_camera, std::move(socket), _manager, [this, self = shared_from_this()](WebsocketSessionPtr &&ptr) {
                    destroySession(std::move(ptr));
              },
              addr, ++_last_session_number, _read_write_timeout
        );
        WebsocketSessionPtr session_to_remove = nullptr;
        auto &session_mutex = is_camera ? _camera_mutex : _headset_mutex;
        auto &session_pool = is_camera ? _camera_sessions : _headset_sessions;
        {
            std::unique_lock lk(session_mutex);
            if (auto current_session = session_pool.find(addr); current_session != session_pool.end()) {
                session_to_remove = std::move(current_session->second);
            }
            session_pool[addr] = session;
        }
        session->Start();
        if (session_to_remove != nullptr) {
            session_to_remove->TryClose(false);
        }
    }

    void WebsocketServer::destroySession(WebsocketSessionPtr &&session) {
        auto &session_mutex = session->_is_camera ? _camera_mutex : _headset_mutex;
        auto &session_pool = session->_is_camera ? _camera_sessions : _headset_sessions;
        std::unique_lock lk(session_mutex);
        if (
            auto dead_session = session_pool.find(session->_addr);
            dead_session != session_pool.end() &&
            dead_session->second->_session_number == session->_session_number
        ) {
            session_pool.erase(dead_session);
        }
    }

    void WebsocketServer::Stop() {
        if (!_is_stopped) {
            _is_stopped = true;
            std::promise<void> done_promise;
            auto done_future = done_promise.get_future();
            auto self(shared_from_this());
            net::post(
                _acceptor.get_executor(),
                [this, self, p = std::move(done_promise)]() mutable {
                    _acceptor.cancel();
                    p.set_value();
                }
            );
            done_future.wait();

            std::vector<WebsocketSessionPtr> removed_sessions;
            {
                std::unique_lock lk1(_camera_mutex, std::defer_lock);
                std::unique_lock lk2(_headset_mutex, std::defer_lock);
                std::lock(lk1, lk2);
                for (auto &[_, session]: _camera_sessions) {
                    removed_sessions.push_back(std::move(session));
                }
                _camera_sessions.clear();
                for (auto &[_, session]: _headset_sessions) {
                    removed_sessions.push_back(std::move(session));
                }
                _headset_sessions.clear();
            }
            for (auto &session: removed_sessions) {
                session->TryClose(false);
            }
        }
    }

    WebsocketServer::~WebsocketServer() {
        std::cout << "WebsocketServer Deconstructing" << std::endl;
        Stop();
    }

    WebsocketSession::WebsocketSession(
        const bool is_camera, tcp::socket &&socket, std::shared_ptr<WebsocketServerManager> &_manager,
        DestroyHandler &&destroy_callback, tcp_addr addr, const unsigned long session_number,
        const int op_timeout
    ):
        _is_camera(is_camera), _ws(std::move(socket)), _manager(_manager),
        _destroy_callback(std::move(destroy_callback)), _addr(std::move(addr)), _session_number(session_number),
        _op_timeout(op_timeout)
    {}

    void WebsocketSession::Start() {
        if (!_is_live) return;

        net::dispatch(
            _ws.get_executor(),
            beast::bind_front_handler(
                &WebsocketSession::onStart,
                shared_from_this()
            )
        );
    }

    void WebsocketSession::onStart() {
        if (!_is_live) return;

        _ws.set_option(
            websocket::stream_base::timeout{
                .handshake_timeout = std::chrono::seconds(5),
                .idle_timeout = std::chrono::seconds(10),
                .keep_alive_pings = true
            }
        );

        _ws.set_option(websocket::stream_base::decorator(
                [](websocket::response_type& res)
                {
                    res.set(http::field::server,
                            std::string(BOOST_BEAST_VERSION_STRING) +
                            " websocket-server-async");
                }
        ));

        _ws.async_accept(
            beast::bind_front_handler(
                &WebsocketSession::onAccept, shared_from_this()
            )
        );
    }

    void WebsocketSession::onAccept(beast::error_code ec) {
        if (!_is_live) return;

        if (ec) {
            std::cout << "WebsocketSession::onAccept; encountered error " << ec << "; bailing" << std::endl;
            TryClose(true);
            return;
        }

        read();
    }

    void WebsocketSession::read() {
        if (!_is_live) return;

        _ws.async_read(
            _read_buffer,
            beast::bind_front_handler(
                &WebsocketSession::onRead,
                shared_from_this()
             )
        );
    }

    void WebsocketSession::onRead(beast::error_code ec, std::size_t bytes_transferred) {
        if (!_is_live) return;

        if (ec) {
            std::cout << "WebsocketSession::onRead; encountered error " << ec << "; bailing" << std::endl;
            TryClose(true);
            return;
        } else if (bytes_transferred == 0) {
            // probably a ping
            read();
            return;
        }

        auto buffers = _read_buffer.data();
        try {
            auto js = nlohmann::json::parse(net::buffers_begin(buffers), net::buffers_end(buffers));
            const bool success = _manager->PostWebsocketMessage(_is_camera, _addr, std::move(js));
            if (success) {
                _read_buffer.consume(bytes_transferred);
                read();
            } else {
                std::cout << "WebsocketSession::onRead failed to send valid json; bailing" << std::endl;
                TryClose(true);
            }
        } catch (nlohmann::json::exception& e) {
            std::cout << "WebsocketSession::onRead failed to parse json with err:" << e.what()
                << "; bailing" << std::endl;
            TryClose(true);
        }
    }

    void WebsocketSession::PostMessage(nlohmann::json &&message) {
        if (!_is_live) return;

        auto message_out = std::make_shared<std::string>(message.dump());
        _ws.async_write(
            net::buffer(*message_out),
            beast::bind_front_handler(&WebsocketSession::onWrite, shared_from_this(), message_out)
        );
    }

    void WebsocketSession::onWrite(
        std::shared_ptr<std::string> message, beast::error_code ec, std::size_t bytes_transferred
    ) {
        if (!_is_live) return;
        if (ec) {
            std::cout << "WebsocketSession::onWrite failed to with ec: " << ec << "; bailing" << std::endl;
            TryClose(true);
        } else if (message->size() != bytes_transferred) {
            std::cout << "WebsocketSession::onWrite only sent " << bytes_transferred << " of " << message->size()
                << " bytes; bailing" << std::endl;
            TryClose(true);
        }
    }



    void WebsocketSession::TryClose(bool internal_close) {
        if (!_is_live) return;
        _is_live = false;
        if (internal_close) {
            auto self(shared_from_this());
            _destroy_callback(std::move(self));
        }
        _ws.async_close(
            websocket::close_code::normal,
            beast::bind_front_handler(&WebsocketSession::onClose, shared_from_this(), internal_close)
         );
    }

    void WebsocketSession::onClose(bool internal_close, beast::error_code ec) {
        if (ec) {
            std::cout << "WebsocketSession::onClose failed to close gracefully" << std::endl;
        }
    }

    WebsocketSession::~WebsocketSession() {
        std::cout << "WebsocketSession Deconstructing" << std::endl;
        if (!_is_live) return;

        // duplicating code, but w.e for now
        _is_live = false;
        std::promise<void> done_promise;
        auto done_future = done_promise.get_future();
        _ws.async_close(
            websocket::close_code::normal,
            [this, self = shared_from_this(), p = std::move(done_promise)](beast::error_code ec) mutable {
                _destroy_callback(std::move(self));
                p.set_value();
            }
         );
        done_future.wait();
    }

}
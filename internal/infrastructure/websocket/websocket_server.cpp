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

        }

        if (ec != net::error::operation_aborted) {
            acceptConnections();
        }

    }

    void WebsocketServer::handleConnection(const bool is_camera, const tcp_addr &addr, tcp::socket &&socket) {
        auto session = std::make_shared<WebsocketSession>(
              is_camera, std::move(socket), _manager, addr, ++_last_session_number, _read_write_timeout
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
        session->Run();
        if (session_to_remove != nullptr) {
            session_to_remove->TryClose(false);
        }
    }

    void WebsocketServer::DestroySession(WebsocketSessionPtr &&session) {
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
        Stop();
    }

}
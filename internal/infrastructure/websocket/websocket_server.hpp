//
// Created by brucegoose on 5/29/23.
//

#ifndef SERVICE_WEBSOCKET_SERVER_HPP
#define SERVICE_WEBSOCKET_SERVER_HPP

#include <json.hpp>
#include <map>
#include <shared_mutex>

#include "utils/asio_context.hpp"

#include "websocket_utils.hpp"

namespace infrastructure {

    class WebsocketServerManager {
    public:
        // tcp server
        [[nodiscard]] virtual ConnectionType GetConnectionType(const tcp::endpoint &endpoint) = 0;
        virtual void PostWebsocketMessage(const tcp_addr addr, nlohmann::json &&message) = 0;
    };

    class WebsocketSession: public std::enable_shared_from_this<WebsocketSession> {
    public:
        WebsocketSession(
            const bool is_camera, tcp::socket &&socket, std::shared_ptr<WebsocketServerManager> &_manager,
            const tcp_addr addr, const unsigned long session_number, const int op_timeout
        );
        WebsocketSession() = delete;
        WebsocketSession (const WebsocketSession&) = delete;
        WebsocketSession& operator= (const WebsocketSession&) = delete;
        ~WebsocketSession();
    protected:
        friend class WebsocketServer;

        void PostMessage(nlohmann::json &&message);
        void Run();
        void TryClose(bool internal_close);

        const tcp_addr _addr;
        const bool _is_camera;
        const unsigned long _session_number;
    private:
        std::shared_ptr<WebsocketServerManager> &_manager;
    };
    typedef std::shared_ptr<WebsocketSession> WebsocketSessionPtr;

    struct WebsocketServerConfig {
        [[nodiscard]] virtual int get_websocket_server_port() const = 0;
        [[nodiscard]] virtual int get_websocket_server_timeout() const = 0;
    };

    class WebsocketServer: public std::enable_shared_from_this<WebsocketServer> {
    public:
        static std::shared_ptr<WebsocketServer> Create(
                const WebsocketServerConfig &config, net::io_context &context, std::shared_ptr<WebsocketServerManager> manager
        ) {
            return std::make_shared<WebsocketServer>(config, context, std::move(manager));
        }
        WebsocketServer() = delete;
        WebsocketServer (const WebsocketServer&) = delete;
        WebsocketServer& operator= (const WebsocketServer&) = delete;
        WebsocketServer(
            const WebsocketServerConfig &config, net::io_context &context, std::shared_ptr<WebsocketServerManager> manager
        );
        void Start();
        void Stop();
        ~WebsocketServer();
    protected:
        friend class WebsocketSession;
        void DestroySession(WebsocketSessionPtr &&session);
    private:
        void acceptConnections();
        void onAccept(beast::error_code ec, tcp::socket socket);
        void handleConnection(const bool is_camera, const tcp_addr &addr, tcp::socket &&socket);
        std::atomic<bool> _is_stopped = { true };
        net::io_context &_context;
        tcp::acceptor _acceptor;
        tcp::endpoint _endpoint;
        std::shared_ptr<WebsocketServerManager> _manager;
        const int _read_write_timeout;

        mutable std::shared_mutex _camera_mutex;
        std::map<tcp_addr, WebsocketSessionPtr> _camera_sessions;
        mutable std::shared_mutex _headset_mutex;
        std::map<tcp_addr, WebsocketSessionPtr> _headset_sessions;
        std::atomic<unsigned long> _last_session_number = { 0 };

    };

}

#endif //SERVICE_WEBSOCKET_SERVER_HPP

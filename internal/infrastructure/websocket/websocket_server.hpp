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
        [[nodiscard]] virtual bool PostWebsocketMessage(
            const bool is_camera, const tcp_addr addr, nlohmann::json &&message
        ) = 0;
    };


    class WebsocketSession;
    typedef std::shared_ptr<WebsocketSession> WebsocketSessionPtr;
    typedef std::function<void(WebsocketSessionPtr &&)> DestroyHandler;

    class WebsocketSession: public std::enable_shared_from_this<WebsocketSession> {
    public:
        WebsocketSession(
            const bool is_camera, tcp::socket &&socket, std::shared_ptr<WebsocketServerManager> &_manager,
            DestroyHandler &&destroy_callback, const tcp_addr addr, const unsigned long session_number,
            const int op_timeout
        );
        WebsocketSession() = delete;
        WebsocketSession (const WebsocketSession&) = delete;
        WebsocketSession& operator= (const WebsocketSession&) = delete;
        ~WebsocketSession();
    protected:
        friend class WebsocketServer;

        void PostMessage(nlohmann::json &&message);
        void Start();
        void TryClose(bool internal_close);

        const bool _is_camera;
        const tcp_addr _addr;
        const unsigned long _session_number;
    private:
        void onStart();
        void onAccept(beast::error_code ec);
        void read();
        void onRead(beast::error_code ec, std::size_t bytes_transferred);
        void onWrite(std::shared_ptr<std::string> message, beast::error_code ec, std::size_t bytes_transferred);
        void onClose(bool internal_close, beast::error_code ec);

        const int _op_timeout;

        DestroyHandler _destroy_callback;
        std::shared_ptr<WebsocketServerManager> &_manager;
        websocket::stream<beast::tcp_stream> _ws;
        beast::flat_buffer _read_buffer;
        std::atomic<bool> _is_live = { true };
    };



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
    private:
        void acceptConnections();
        void onAccept(beast::error_code ec, tcp::socket socket);
        void handleConnection(const bool is_camera, const tcp_addr &addr, tcp::socket &&socket);
        void destroySession(WebsocketSessionPtr &&session);
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

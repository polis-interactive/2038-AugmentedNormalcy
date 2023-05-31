//
// Created by brucegoose on 5/29/23.
//

#ifndef SERVICE_WEBSOCKET_CLIENT_HPP
#define SERVICE_WEBSOCKET_CLIENT_HPP

#include <string>
#include <atomic>

#include <json.hpp>

#include "utils/asio_context.hpp"

#include "websocket_utils.hpp"

namespace infrastructure {

    struct WebsocketClientConfig {
        [[nodiscard]] virtual std::string get_websocket_server_host() const = 0;
        [[nodiscard]] virtual int get_websocket_server_port() const = 0;
        [[nodiscard]] virtual int get_websocket_client_connection_timeout() const = 0;
        [[nodiscard]] virtual int get_websocket_client_op_timeout() const = 0;
    };

    class WebsocketClientManager {
    public:
        virtual void CreateWebsocketClientConnection() = 0;
        [[nodiscard]] virtual bool PostWebsocketServerMessage(nlohmann::json &&message) = 0;
        virtual void DestroyWebsocketClientConnection() = 0;
    };
    typedef std::shared_ptr<WebsocketClientManager> WebsocketClientManagerPtr;

    class WebsocketClient;
    typedef std::shared_ptr<WebsocketClient> WebsocketClientPtr;

    class WebsocketClient: public std::enable_shared_from_this<WebsocketClient> {
    public:
        static WebsocketClientPtr Create(
            const WebsocketClientConfig &config, net::io_context &context,
            WebsocketClientManagerPtr manager
        );
        WebsocketClient() = delete;
        WebsocketClient (const WebsocketClient&) = delete;
        WebsocketClient& operator= (const WebsocketClient&) = delete;
        WebsocketClient(
            const WebsocketClientConfig &config, net::io_context &context,
            WebsocketClientManagerPtr &&manager
        );
        void Start();
        void PostWebsocketClientMessage(nlohmann::json &&message);
        void Stop();
    private:
        void startConnection(const unsigned long session_number, bool is_initial_connection);
        void onConnection(const unsigned long session_number, beast::error_code ec);
        void onHandshake(const unsigned long session_number, beast::error_code ec);
        void read(const unsigned long session_number);
        void onRead(const unsigned long session_number, beast::error_code ec, std::size_t bytes_transferred);
        void write(std::string &&message);
        void doWrite(const unsigned long session_number);
        void onWrite(
            const unsigned long session_number, beast::error_code ec, std::size_t bytes_transferred
        );
        void disconnect(const unsigned long session_number);
        void onDisconnect(const unsigned long session_number, beast::error_code ec);

        const int _connection_timeout;
        const int _op_timeout;
        const std::string _host;
        std::atomic<bool> _is_stopped = {true};
        std::atomic<unsigned long> _session_number = { 0 };
        tcp::endpoint _remote_endpoint;

        WebsocketClientManagerPtr _manager;
        net::strand<net::io_context::executor_type> _strand;
        std::shared_ptr<websocket::stream<beast::tcp_stream>> _ws = nullptr;
        beast::flat_buffer _read_buffer;

        std::queue<std::string> _send_queue = {};

    };

}

#endif //SERVICE_WEBSOCKET_CLIENT_HPP

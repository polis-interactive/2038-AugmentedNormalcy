//
// Created by brucegoose on 5/31/23.
//

#ifndef AUGMENTEDNORMALCY_TEST_WEBSOCKET_HPP
#define AUGMENTEDNORMALCY_TEST_WEBSOCKET_HPP

#include <functional>

#include "infrastructure/websocket/websocket_server.hpp"
#include "infrastructure/websocket/websocket_client.hpp"


struct TestServerClientConfig:
        public AsioContextConfig,
        public infrastructure::WebsocketClientConfig,
        public infrastructure::WebsocketServerConfig
{
    [[nodiscard]] int get_asio_pool_size() const override {
        return 3;
    };

    [[nodiscard]] std::string get_websocket_server_host() const override {
        return "127.0.0.1";
    };
    [[nodiscard]] int get_websocket_server_port() const override {
        return 42069;
    };
    [[nodiscard]] int get_websocket_client_connection_timeout() const override {
        return 30;
    };
    [[nodiscard]] int get_websocket_client_op_timeout() const override {
        return 10;
    };

    [[nodiscard]] int get_websocket_server_timeout() const override {
        return 10;
    };

    [[nodiscard]] ConnectionType get_websocket_client_connection_type() const override {
        return ConnectionType::HEADSET_CONNECTION;
    };
    [[nodiscard]] bool get_websocket_client_used_fixed_port() const override {
        return false;
    };
};

class WebsocketClientServerManager:
        public infrastructure::WebsocketServerManager,
        public infrastructure::WebsocketClientManager{
public:
    WebsocketClientServerManager(
        std::function<void(nlohmann::json &&)> server_callback,
        std::function<void(nlohmann::json &&)> client_callback
    ):
        _server_callback(std::move(server_callback)),
        _client_callback(std::move(client_callback))
    {}
    // server
    [[nodiscard]] ConnectionType GetConnectionType(const tcp::endpoint &endpoint) override {
        return ConnectionType::CAMERA_CONNECTION;
    };
    [[nodiscard]] bool PostWebsocketMessage(
            const ConnectionType connection_type, const tcp_addr addr, nlohmann::json &&message
    ) override {
        _server_callback(std::move(message));
        return return_success;
    };
    // client
    [[nodiscard]] bool PostWebsocketServerMessage(nlohmann::json &&message) override {
        _client_callback(std::move(message));
        return return_success;
    };
    void CreateWebsocketClientConnection() override {
        _client_is_connected = true;
    };
    void DestroyWebsocketClientConnection() override {
        _client_is_connected = false;
    };
    void SetRetSuccess(const bool ret_success) {
        return_success = ret_success;
    }
    std::function<void(nlohmann::json &&)> _server_callback;
    std::function<void(nlohmann::json &&)> _client_callback;
    bool return_success = true;
    std::atomic<bool> _client_is_connected;
};


#endif //AUGMENTEDNORMALCY_TEST_WEBSOCKET_HPP

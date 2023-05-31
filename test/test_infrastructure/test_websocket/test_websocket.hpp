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
};

class WebsocketServerManager: public infrastructure::WebsocketServerManager {
public:
    WebsocketServerManager(std::function<void(nlohmann::json &&)> callback):
        _callback(std::move(callback))
    {}
    [[nodiscard]] ConnectionType GetConnectionType(const tcp::endpoint &endpoint) override {
        return ConnectionType::CAMERA_CONNECTION;
    };
    [[nodiscard]] bool PostWebsocketMessage(
            const bool is_camera, const tcp_addr addr, nlohmann::json &&message
    ) {
        _callback(std::move(message));
        return return_success;
    };
    void SetRetSuccess(const bool ret_success) {
        return_success = ret_success;
    }
private:
    std::function<void(nlohmann::json &&)> _callback;
    bool return_success = true;
};

class WebsocketClientManager: public infrastructure::WebsocketClientManager {
public:
    WebsocketClientManager(std::function<void(nlohmann::json &&)> callback):
            _callback(std::move(callback))
    {}
    [[nodiscard]] bool PostWebsocketServerMessage(nlohmann::json &&message) override {
        _callback(std::move(message));
        return return_success;
    };
    void CreateWebsocketClientConnection() override {
        is_connected = true;
    };
    void DestroyWebsocketClientConnection() override {
        is_connected = false;
    };
    void SetRetSuccess(const bool ret_success) {
        return_success = ret_success;
    }
private:
    std::function<void(nlohmann::json &&)> _callback;
    bool return_success = true;
    std::atomic<bool> is_connected;
};

#endif //AUGMENTEDNORMALCY_TEST_WEBSOCKET_HPP

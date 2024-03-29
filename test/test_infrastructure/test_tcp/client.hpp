//
// Created by brucegoose on 3/18/23.
//

#ifndef AUGMENTEDNORMALCY_TEST_TCP_CLIENT_HPP
#define AUGMENTEDNORMALCY_TEST_TCP_CLIENT_HPP

#include <utility>

#include "infrastructure/tcp/tcp_client.hpp"
#include "infrastructure/tcp/tcp_server.hpp"

struct TestClientServerConfig:
        public AsioContextConfig,
        public infrastructure::TcpClientConfig,
        public infrastructure::TcpServerConfig
{
    explicit TestClientServerConfig(
        int pool_size, int tcp_server_port, std::string tcp_server_host, ConnectionType tcp_client_connection_type
    ):
            _pool_size(pool_size),
            _tcp_server_port(tcp_server_port),
            _tcp_server_host(std::move(tcp_server_host)),
            _tcp_client_connection_type(tcp_client_connection_type)
    {}
    const int _pool_size;
    const int _tcp_server_port;
    const std::string _tcp_server_host;
    const ConnectionType _tcp_client_connection_type;
    [[nodiscard]] int get_asio_pool_size() const override {
        return _pool_size;
    };
    [[nodiscard]] int get_tcp_server_port() const override {
        return _tcp_server_port;
    };
    [[nodiscard]] std::string get_tcp_server_host() const override {
        return _tcp_server_host;
    };
    [[nodiscard]] ConnectionType get_tcp_client_connection_type() const override {
        return _tcp_client_connection_type;
    };
    [[nodiscard]] bool get_tcp_client_used_fixed_port() const override {
        return false;
    };
    [[nodiscard]] int get_tcp_server_timeout() const override {
        return 10;
    }
    [[nodiscard]] int get_tcp_client_timeout_on_read() const override {
        return 10;
    };
    [[nodiscard]] int get_tcp_camera_session_buffer_count() const override {
        return 8;
    }
    [[nodiscard]] int get_tcp_headset_session_buffer_count() const override {
        return 5;
    }
    [[nodiscard]] int get_tcp_server_buffer_size() const override {
        return 5;
    }
    [[nodiscard]] int get_tcp_client_read_buffer_count() const override {
        return 5;
    };
    [[nodiscard]] int get_tcp_client_read_buffer_size() const override {
        return 1536 * 864 * 3 / 2;
    };
};

class TcpClientManager: public infrastructure::TcpClientManager {
public:
    explicit TcpClientManager() {
    }
    // camera session
    void CreateCameraClientConnection() override {};
    void DestroyCameraClientConnection() override {};

    // headset session
    void CreateHeadsetClientConnection() override {
        is_headset_connected = true;
    };
    void PostHeadsetClientBuffer(std::shared_ptr<SizedBuffer> &&buffer) override {}
    void DestroyHeadsetClientConnection() override {
        is_headset_connected = false;
    };
    std::atomic_bool is_headset_connected = false;
};

#endif //AUGMENTEDNORMALCY_TEST_TCP_CLIENT_HPP

//
// Created by brucegoose on 3/18/23.
//

#ifndef AUGMENTEDNORMALCY_TEST_TCP_CLIENT_HPP
#define AUGMENTEDNORMALCY_TEST_TCP_CLIENT_HPP

#include <utility>

#include "infrastructure/tcp/tcp_client.hpp"
#include "infrastructure/tcp/tcp_server.hpp"

struct TestClientServerConfig:
        public infrastructure::TcpContextConfig,
        public infrastructure::TcpClientConfig,
        public infrastructure::TcpServerConfig
{
    explicit TestClientServerConfig(
        int pool_size, int tcp_server_port, std::string tcp_server_host, bool tcp_client_is_camera
    ):
            _pool_size(pool_size),
            _tcp_server_port(tcp_server_port),
            _tcp_server_host(std::move(tcp_server_host)),
            _tcp_client_is_camera(tcp_client_is_camera)
    {}
    const int _pool_size;
    const int _tcp_server_port;
    const std::string _tcp_server_host;
    const bool _tcp_client_is_camera;
    [[nodiscard]] int get_tcp_pool_size() const override {
        return _pool_size;
    };
    [[nodiscard]] int get_tcp_server_port() const override {
        return _tcp_server_port;
    };
    [[nodiscard]] std::string get_tcp_server_host() const override {
        return _tcp_server_host;
    };
    [[nodiscard]] bool get_tcp_client_is_camera() const override {
        return _tcp_client_is_camera;
    };
};

class TcpClientManager: public infrastructure::TcpClientManager {
public:
    explicit TcpClientManager(std::shared_ptr<ResizableBufferPool> optional_buffer_pool) {
        if (optional_buffer_pool != nullptr) {
            _optional_pushing_buffer_pool = std::move(optional_buffer_pool);
        }
    }
    // camera session
    void CreateCameraClientConnection() override {};
    void DestroyCameraClientConnection() override {};

    // headset session
    std::shared_ptr<ResizableBufferPool> CreateHeadsetClientConnection() override {
        is_headset_connected = true;
        return _optional_pushing_buffer_pool;
    };
    void DestroyHeadsetClientConnection() override {
        is_headset_connected = false;
    };
    SizedBufferCallback _write_call;
    std::shared_ptr<ResizableBufferPool> _optional_pushing_buffer_pool = nullptr;
    std::atomic_bool is_headset_connected = false;
};

#endif //AUGMENTEDNORMALCY_TEST_TCP_CLIENT_HPP

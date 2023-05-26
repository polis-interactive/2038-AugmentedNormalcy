//
// Created by brucegoose on 3/18/23.
//

#ifndef AUGMENTEDNORMALCY_TEST_TCP_SERVER_HPP
#define AUGMENTEDNORMALCY_TEST_TCP_SERVER_HPP

#include "infrastructure/tcp/tcp_server.hpp"

struct TestServerConfig:
        public infrastructure::TcpContextConfig,
        public infrastructure::TcpServerConfig
{
    explicit TestServerConfig(int pool_size, int tcp_server_port):
            _pool_size(pool_size),
            _tcp_server_port(tcp_server_port)
    {}
    const int _pool_size;
    const int _tcp_server_port;
    [[nodiscard]] int get_tcp_pool_size() const override {
        return _pool_size;
    };
    [[nodiscard]] int get_tcp_server_port() const override {
        return _tcp_server_port;
    };
    [[nodiscard]] int get_tcp_server_timeout() const override {
        return 1;
    }
    [[nodiscard]] int get_tcp_camera_session_buffer_count() const override {
        return 1;
    };
    [[nodiscard]] int get_tcp_headset_session_buffer_count() const override {
        return 1;
    };
    [[nodiscard]] int get_tcp_server_buffer_size() const override {
        return 5;
    };
};

/* Used to test bringing up and tearing down the server */

class NoSessionManager: public infrastructure::TcpServerManager {
public:
    [[nodiscard]] infrastructure::TcpConnectionType GetConnectionType(const tcp::endpoint &endpoint) override {
        return infrastructure::TcpConnectionType::UNKNOWN_CONNECTION;
    }
    /* dummy's for the rest */
    [[nodiscard]] unsigned long CreateCameraServerConnection(
        std::shared_ptr<infrastructure::TcpSession> &&session
    ) override {
        return 1;
    }
    void PostCameraServerBuffer(const tcp_addr &addr, std::shared_ptr<ResizableBuffer> &&buffer) override {};
    void DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> &&session) override {}
    [[nodiscard]] unsigned long CreateHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> &&session
    ) override {
        return 0;
    }
    void DestroyHeadsetServerConnection(std::shared_ptr<infrastructure::WritableTcpSession> &&session) override {}
};

#endif //AUGMENTEDNORMALCY_TEST_TCP_SERVER_HPP

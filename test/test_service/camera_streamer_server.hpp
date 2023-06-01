//
// Created by brucegoose on 3/23/23.
//

#ifndef AUGMENTEDNORMALCY_TEST_CAMERA_STREAMER_SERVER_HPP
#define AUGMENTEDNORMALCY_TEST_CAMERA_STREAMER_SERVER_HPP

#include <deque>
#include <iostream>

#include "infrastructure/tcp/tcp_server.hpp"
#include "utils/buffers.hpp"

struct TestServerConfig:
        public AsioContextConfig,
        public infrastructure::TcpServerConfig
{
    explicit TestServerConfig(int pool_size, int tcp_server_port):
            _pool_size(pool_size),
            _tcp_server_port(tcp_server_port)
    {}
    const int _pool_size;
    const int _tcp_server_port;
    [[nodiscard]] int get_asio_pool_size() const override {
        return _pool_size;
    };
    [[nodiscard]] int get_tcp_server_port() const override {
        return _tcp_server_port;
    };
    [[nodiscard]] int get_tcp_server_timeout() const override {
        return 5;
    }
    [[nodiscard]] int get_tcp_camera_session_buffer_count() const override {
        return 4;
    };
    [[nodiscard]] int get_tcp_headset_session_buffer_count() const override {
        return 4;
    }
    [[nodiscard]] int get_tcp_server_buffer_size() const override {
        return 1990656;
    };
};


class TcpCameraServerManager:
        public infrastructure::TcpServerManager
{
public:
    explicit TcpCameraServerManager(ResizableBufferCallback on_receive):
        _on_receive(std::move(on_receive))
    {}

    /* camera server */
    [[nodiscard]] ConnectionType GetConnectionType(const tcp::endpoint &endpoint) override {
        return ConnectionType::CAMERA_CONNECTION;
    }
    [[nodiscard]]  unsigned long CreateCameraServerConnection(
        std::shared_ptr<infrastructure::TcpSession> &&session
    ) override {
        client_is_connected = true;
        return 0;
    };
    void PostCameraServerBuffer(const tcp_addr &addr, std::shared_ptr<ResizableBuffer> &&buffer) {
        _on_receive(std::move(buffer));
    }
    ResizableBufferCallback _on_receive;
    void DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> &&session) override {
        client_is_connected = false;
    }
    [[nodiscard]] bool ClientIsConnected() const {
        return client_is_connected;
    }
    std::atomic_bool client_is_connected = false;

    /* dummy for headset server */
    unsigned long CreateHeadsetServerConnection(std::shared_ptr<infrastructure::WritableTcpSession> &&session) override {
        return 0;
    }
    void DestroyHeadsetServerConnection(std::shared_ptr<infrastructure::WritableTcpSession> &&session) override {}
};


#endif //AUGMENTEDNORMALCY_TEST_CAMERA_STREAMER_SERVER_HPP

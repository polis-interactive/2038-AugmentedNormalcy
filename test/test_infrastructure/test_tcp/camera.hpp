//
// Created by brucegoose on 3/18/23.
//

#ifndef AUGMENTEDNORMALCY_TEST_TCP_CAMERA_HPP
#define AUGMENTEDNORMALCY_TEST_TCP_CAMERA_HPP

#include "infrastructure/tcp/tcp_client.hpp"
#include "infrastructure/tcp/tcp_server.hpp"

class TcpCameraClientServerManager:
        public infrastructure::TcpClientManager,
        public infrastructure::TcpServerManager
{
public:
    explicit TcpCameraClientServerManager(
        std::shared_ptr<SizedPlaneBufferPool> buffer_pool
    ):
        _buffer_pool(std::move(buffer_pool))
    {}
    /* camera client */
    void CreateCameraClientConnection() override {
        client_is_connected = true;
    };
    void DestroyCameraClientConnection() override {
        client_is_connected = false;
    };
    [[nodiscard]] bool ClientIsConnected() const {
        return client_is_connected;
    }
    std::atomic_bool client_is_connected = false;


    /* camera server */
    [[nodiscard]] infrastructure::TcpConnectionType GetConnectionType(tcp::endpoint endpoint) override {
        return infrastructure::TcpConnectionType::CAMERA_CONNECTION;
    }
    [[nodiscard]]  infrastructure::CameraConnectionPayload CreateCameraServerConnection(
        std::shared_ptr<infrastructure::TcpSession> session
    ) override {
        return { 0, _buffer_pool };
    };
    void DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> session) override {}
    std::shared_ptr<SizedPlaneBufferPool> _buffer_pool;

    /* dummy for headset server */
    [[nodiscard]] unsigned long CreateHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> session
    ) override {
        return 0;
    }
    void DestroyHeadsetServerConnection(std::shared_ptr<infrastructure::WritableTcpSession> session) override {}

    /* dummy for headset client */
    std::shared_ptr<SizedBufferPool> CreateHeadsetClientConnection() override {
        return nullptr;
    };
    void PostHeadsetClientBuffer(std::shared_ptr<SizedBuffer> &&buffer) override {};
    void DestroyHeadsetClientConnection() override {};
};

#endif //AUGMENTEDNORMALCY_TEST_TCP_CAMERA_HPP

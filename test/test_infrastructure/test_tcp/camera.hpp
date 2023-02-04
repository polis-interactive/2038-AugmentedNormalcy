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
    explicit TcpCameraClientServerManager(std::shared_ptr<PushingBufferPool> buffer_pool)
            : _buffer_pool(std::move(buffer_pool))
    {}
    /* camera client */
    void CreateCameraClientConnection(SizedBufferCallback write_call) override {
        client_is_connected = true;
        _write_call = std::move(write_call);
    };
    void DestroyCameraClientConnection() override {
        client_is_connected = false;
        _write_call = nullptr;
    };
    [[nodiscard]] bool ClientIsConnected() const {
        return client_is_connected;
    }
    std::atomic_bool client_is_connected = false;
    SizedBufferCallback _write_call;


    /* camera server */
    [[nodiscard]] infrastructure::TcpConnectionType GetConnectionType(tcp::endpoint endpoint) override {
        return infrastructure::TcpConnectionType::CAMERA_CONNECTION;
    }
    [[nodiscard]]  infrastructure::CameraConnectionPayload CreateCameraServerConnection(tcp::endpoint endpoint) override {
        return { 0, _buffer_pool };
    };
    void DestroyCameraServerConnection(tcp::endpoint endpoint, unsigned long session_id) override {}
    std::shared_ptr<PushingBufferPool> _buffer_pool;

    /* dummy for headset server */
    [[nodiscard]] unsigned long CreateHeadsetServerConnection(
            tcp::endpoint endpoint, SizedBufferCallback writeCall
    ) override {
        return 0;
    }
    void DestroyHeadsetServerConnection(tcp::endpoint endpoint, unsigned long session_id) override {}

    /* dummy for headset client */
    std::shared_ptr<PushingBufferPool> CreateHeadsetClientConnection() override {
        return nullptr;
    };
    void DestroyHeadsetClientConnection() override {};
};

#endif //AUGMENTEDNORMALCY_TEST_TCP_CAMERA_HPP

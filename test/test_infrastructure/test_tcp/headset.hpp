//
// Created by brucegoose on 3/18/23.
//

#ifndef AUGMENTEDNORMALCY_TEST_TCP_HEADSET_HPP
#define AUGMENTEDNORMALCY_TEST_TCP_HEADSET_HPP

#include "infrastructure/tcp/tcp_client.hpp"
#include "infrastructure/tcp/tcp_server.hpp"

class TcpHeadsetClientServerManager:
        public infrastructure::TcpClientManager,
        public infrastructure::TcpServerManager
{
public:
    explicit TcpHeadsetClientServerManager(std::shared_ptr<PushingBufferPool> buffer_pool)
            : _buffer_pool(std::move(buffer_pool))
    {}

    /* headset client */
    std::shared_ptr<PushingBufferPool> CreateHeadsetClientConnection() override {
        client_is_connected = true;
        return _buffer_pool;
    };
    void DestroyHeadsetClientConnection() override {
        client_is_connected = false;
    };
    [[nodiscard]] bool ClientIsConnected() const {
        return client_is_connected;
    }
    std::atomic_bool client_is_connected = false;
    std::shared_ptr<PushingBufferPool> _buffer_pool;

    /* headset server */
    [[nodiscard]] infrastructure::TcpConnectionType GetConnectionType(tcp::endpoint endpoint) override {
        return infrastructure::TcpConnectionType::HEADSET_CONNECTION;
    }
    [[nodiscard]] unsigned long CreateHeadsetServerConnection(
            tcp::endpoint endpoint, infrastructure::HeadsetWriteCall write_call
    ) override {
        _write_call = std::move(write_call);
        return 0;
    }
    void DestroyHeadsetServerConnection(tcp::endpoint endpoint, unsigned long session_id) override {
        _write_call = nullptr;
    }
    infrastructure::CameraWriteCall _write_call;


    /* dummy for camera client */
    void CreateCameraClientConnection(infrastructure::CameraWriteCall write_call) override {};
    void DestroyCameraClientConnection() override {};

    /* dummy for camera server */
    [[nodiscard]]  infrastructure::CameraConnectionPayload CreateCameraServerConnection(tcp::endpoint endpoint) override {
        return { 0, nullptr };
    };
    void DestroyCameraServerConnection(tcp::endpoint endpoint, unsigned long session_id) override {}
};

#endif //AUGMENTEDNORMALCY_TEST_TCP_HEADSET_HPP

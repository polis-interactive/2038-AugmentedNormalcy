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
        std::shared_ptr<SizedBufferPool> buffer_pool,
        std::function<void(std::shared_ptr<SizedBuffer> &&)> callback
    ):
        _buffer_pool(std::move(buffer_pool)),
        _callback(std::move(callback))
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
    void PostCameraServerBuffer(std::shared_ptr<SizedBuffer> &&buffer) override {
        _callback(std::move(buffer));
    };
    [[nodiscard]]  infrastructure::CameraConnectionPayload CreateCameraServerConnection(tcp::endpoint endpoint) override {
        return { 0, _buffer_pool };
    };
    void DestroyCameraServerConnection(tcp::endpoint endpoint, unsigned long session_id) override {}
    std::shared_ptr<SizedBufferPool> _buffer_pool;
    std::function<void(std::shared_ptr<SizedBuffer> &&)> _callback;

    /* dummy for headset server */
    [[nodiscard]] unsigned long CreateHeadsetServerConnection(
            tcp::endpoint endpoint, SizedBufferCallback writeCall
    ) override {
        return 0;
    }
    void PostHeadsetClientBuffer(std::shared_ptr<SizedBuffer> &&buffer) override {};
    void DestroyHeadsetServerConnection(tcp::endpoint endpoint, unsigned long session_id) override {}

    /* dummy for headset client */
    std::shared_ptr<SizedBufferPool> CreateHeadsetClientConnection() override {
        return nullptr;
    };
    void DestroyHeadsetClientConnection() override {};
};

#endif //AUGMENTEDNORMALCY_TEST_TCP_CAMERA_HPP

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
    explicit TcpHeadsetClientServerManager(
        std::shared_ptr<ResizableBufferPool> buffer_pool
    ) :
        _buffer_pool(std::move(buffer_pool))
    {}

    /* headset client */
    std::shared_ptr<ResizableBufferPool> CreateHeadsetClientConnection() override {
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
    std::shared_ptr<ResizableBufferPool> _buffer_pool;

    /* headset server */
    [[nodiscard]] infrastructure::TcpConnectionType GetConnectionType(tcp_addr addr) override {
        return infrastructure::TcpConnectionType::HEADSET_CONNECTION;
    }
    [[nodiscard]] unsigned long CreateHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> session
    ) override {
        _session = std::move(session);
        return 0;
    }
    void DestroyHeadsetServerConnection(std::shared_ptr<infrastructure::WritableTcpSession> session) override {
        _session = nullptr;
    }
    std::shared_ptr<infrastructure::WritableTcpSession> _session;


    /* dummy for camera client */
    void CreateCameraClientConnection() override {};
    void DestroyCameraClientConnection() override {};

    /* dummy for camera server */
    [[nodiscard]]  infrastructure::CameraConnectionPayload CreateCameraServerConnection(
        std::shared_ptr<infrastructure::TcpSession> session
    ) override {
        return { 0, nullptr };
    };
    void DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> session) override {}
};

#endif //AUGMENTEDNORMALCY_TEST_TCP_HEADSET_HPP

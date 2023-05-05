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
        ResizableBufferCallback &&on_receive
    ):
        _on_receive(std::move(on_receive))
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
    [[nodiscard]] infrastructure::TcpConnectionType GetConnectionType(tcp_addr addr) override {
        return infrastructure::TcpConnectionType::CAMERA_CONNECTION;
    }
    [[nodiscard]] unsigned long CreateCameraServerConnection(
        std::shared_ptr<infrastructure::TcpSession> session
    ) override {
        return 0;
    };
    void PostCameraServerBuffer(const tcp_addr &addr, std::shared_ptr<ResizableBuffer> &&buffer) override {
        _on_receive(std::move(buffer));
    }
    void DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> session) override {}
    ResizableBufferCallback _on_receive;

    /* dummy for headset server */
    [[nodiscard]] unsigned long CreateHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> session
    ) override {
        return 0;
    }
    void DestroyHeadsetServerConnection(std::shared_ptr<infrastructure::WritableTcpSession> session) override {}

    /* dummy for headset client */
    std::shared_ptr<ResizableBufferPool> CreateHeadsetClientConnection() override {
        return nullptr;
    };
    void DestroyHeadsetClientConnection() override {};
};

#endif //AUGMENTEDNORMALCY_TEST_TCP_CAMERA_HPP

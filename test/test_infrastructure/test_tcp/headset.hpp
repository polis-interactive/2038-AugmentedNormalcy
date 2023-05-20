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
    explicit TcpHeadsetClientServerManager(SizedBufferCallback callback) :
            on_receive(std::move(callback))
    {}

    /* headset client */
    void CreateHeadsetClientConnection() override {
        client_is_connected = true;
    };

    void PostHeadsetClientBuffer(std::shared_ptr<SizedBuffer> &&buffer) {
        on_receive(std::move(buffer));
    }

    void DestroyHeadsetClientConnection() override {
        client_is_connected = false;
    };
    [[nodiscard]] bool ClientIsConnected() const {
        return client_is_connected;
    }
    std::atomic_bool client_is_connected = false;
    SizedBufferCallback on_receive;

    /* headset server */
    [[nodiscard]] infrastructure::TcpConnectionType GetConnectionType(const tcp_addr &addr) override {
        return infrastructure::TcpConnectionType::HEADSET_CONNECTION;
    }
    [[nodiscard]] unsigned long CreateHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> &&session
    ) override {
        _session = std::move(session);
        return 0;
    }
    void DestroyHeadsetServerConnection(std::shared_ptr<infrastructure::WritableTcpSession> &&session) override {
        _session = nullptr;
    }
    std::shared_ptr<infrastructure::WritableTcpSession> _session;


    /* dummy for camera client */
    void CreateCameraClientConnection() override {};
    void DestroyCameraClientConnection() override {};

    /* dummy for camera server */
    [[nodiscard]]  unsigned long CreateCameraServerConnection(
        std::shared_ptr<infrastructure::TcpSession> &&session
    ) override {
        return 0;
    };
    void PostCameraServerBuffer(const tcp_addr &addr, std::shared_ptr<ResizableBuffer> &&buffer) override {}
    void DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> &&session) override {}
};

#endif //AUGMENTEDNORMALCY_TEST_TCP_HEADSET_HPP

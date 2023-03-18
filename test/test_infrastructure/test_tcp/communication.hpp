//
// Created by brucegoose on 3/18/23.
//

#ifndef AUGMENTEDNORMALCY_TEST_TCP_COMMUNICATION_HPP
#define AUGMENTEDNORMALCY_TEST_TCP_COMMUNICATION_HPP

#include <utility>
#include <deque>

#include "infrastructure/tcp/tcp_client.hpp"
#include "infrastructure/tcp/tcp_server.hpp"
#include "utils/buffer_pool/buffer_pool.hpp"

class TcpCameraClientServerManager:
        public infrastructure::TcpClientManager,
        public infrastructure::TcpServerManager
{
public:
    explicit TcpCameraClientServerManager(std::shared_ptr<PushingBufferPool> buffer_pool)
        : _buffer_pool(std::move(buffer_pool))
    {}
    /* camera client */
    void CreateCameraClientConnection(infrastructure::CameraWriteCall write_call) override {
        client_is_connected = true;
        _write_call = std::move(write_call);
    };
    void DestroyCameraClientConnection() override {
        client_is_connected = false;
        _write_call = nullptr;
    };
    bool ClientIsConnected() {
        return client_is_connected;
    }
    std::atomic_bool client_is_connected = false;
    infrastructure::CameraWriteCall _write_call;


    /* camera server */
    [[nodiscard]] infrastructure::TcpConnectionType GetConnectionType(tcp::endpoint endpoint) override {
        return infrastructure::TcpConnectionType::CAMERA_CONNECTION;
    }
    [[nodiscard]]  infrastructure::CameraConnectionPayload CreateCameraServerConnection(tcp::endpoint endpoint) {
        return { 0, _buffer_pool };
    };
    void DestroyCameraServerConnection(tcp::endpoint endpoint, unsigned long session_id) {}
    std::shared_ptr<PushingBufferPool> _buffer_pool;

    /* dummy for headset server */
    [[nodiscard]] unsigned long CreateHeadsetServerConnection(
            tcp::endpoint endpoint, infrastructure::HeadsetWriteCall writeCall
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

class FakeSizedBuffer: public SizedBuffer {
public:
    explicit FakeSizedBuffer(unsigned int buffer_size):
        _buffer_size(buffer_size)
    {
        _buffer = new char[buffer_size];
    };
    explicit FakeSizedBuffer(std::string &str):
        _buffer(new char[str.size()]),
        _buffer_size(str.size())
    {
        str.copy(_buffer, 5);
    }
    [[nodiscard]] void *GetMemory() override {
        return _buffer;
    };
    [[nodiscard]] std::size_t GetSize() override {
        return _buffer_size;
    };
    char *_buffer;
    const unsigned int _buffer_size;
};

class FakeBufferPool: public PushingBufferPool {
public:
    explicit FakeBufferPool(
        std::function<void(std::shared_ptr<SizedBuffer> &&buffer)> callback,
        unsigned int buffer_size, unsigned int buffer_count
    ):
        _callback(std::move(callback))
    {
        for (int i = 0; i < buffer_count; i++) {
            _buffers.push_back(new FakeSizedBuffer(buffer_size));
        }
    }
    std::shared_ptr<SizedBuffer> GetSizedBuffer() override {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        auto fake_buffer = _buffers.front();
        _buffers.pop_front();
        auto buffer = std::shared_ptr<SizedBuffer>(
            (SizedBuffer *) fake_buffer, [this, fake_buffer](SizedBuffer *) mutable {
                std::unique_lock<std::mutex> lock(_buffer_mutex);
                _buffers.push_back(fake_buffer);
            }
        );
        return std::move(buffer);
    }
    void SendSizedBuffer(std::shared_ptr<SizedBuffer> &&buffer) override {
        _callback(std::move(buffer));
    }
    std::size_t AvailableBuffers() {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        return _buffers.size();
    }
    std::deque<FakeSizedBuffer *> _buffers;
    std::mutex _buffer_mutex;
    std::function<void(std::shared_ptr<SizedBuffer> &&buffer)> _callback;
};



#endif //AUGMENTEDNORMALCY_TEST_TCP_COMMUNICATION_HPP

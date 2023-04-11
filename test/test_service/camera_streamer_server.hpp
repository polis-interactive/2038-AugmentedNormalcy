//
// Created by brucegoose on 3/23/23.
//

#ifndef AUGMENTEDNORMALCY_TEST_CAMERA_STREAMER_SERVER_HPP
#define AUGMENTEDNORMALCY_TEST_CAMERA_STREAMER_SERVER_HPP

#include <deque>

#include "infrastructure/tcp/tcp_server.hpp"
#include "utils/buffers.hpp"

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
        str.copy(_buffer, _buffer_size);
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

class FakePlaneBuffer: public SizedBufferPool {
public:
    explicit FakePlaneBuffer(unsigned int buffer_size) {
        _buffer = std::make_shared<FakeSizedBuffer>(buffer_size);
    }
    [[nodiscard]] std::shared_ptr<SizedBuffer> GetSizedBuffer() override {
        if (_has_sent) {
            _has_sent = !_has_sent;
            return nullptr;
        } else {
            _has_sent = !_has_sent;
            return _buffer;
        }
    }
    bool _has_sent = false;
    std::shared_ptr<FakeSizedBuffer> _buffer;
};

class FakePlaneBufferPool: public SizedPlaneBufferPool {
public:
    explicit FakePlaneBufferPool(
            unsigned int buffer_size, unsigned int buffer_count, SizedBufferPoolCallback callback
    ):
            _callback(std::move(callback))
    {
        for (int i = 0; i < buffer_count; i++) {
            _buffers.push_back(new FakePlaneBuffer(buffer_size));
        }
    }
    std::shared_ptr<SizedBufferPool> GetSizedBufferPool() override {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        auto fake_buffer = _buffers.front();
        _buffers.pop_front();
        auto buffer = std::shared_ptr<SizedBufferPool>(
                (SizedBufferPool *) fake_buffer, [this, fake_buffer](SizedBufferPool *) mutable {
                    std::unique_lock<std::mutex> lock(_buffer_mutex);
                    _buffers.push_back(fake_buffer);
                }
        );
        return std::move(buffer);
    }
    void PostSizedBufferPool(std::shared_ptr<SizedBufferPool> &&buffer) override {
        _callback(std::move(buffer));
    }
    std::size_t AvailableBuffers() {
        std::unique_lock<std::mutex> lock(_buffer_mutex);
        return _buffers.size();
    }
    std::deque<FakePlaneBuffer *> _buffers;
    std::mutex _buffer_mutex;
    SizedBufferPoolCallback _callback;
};


class TcpCameraServerManager:
        public infrastructure::TcpServerManager
{
public:
    explicit TcpCameraServerManager(std::shared_ptr<SizedPlaneBufferPool> buffer_pool):
        _buffer_pool(std::move(buffer_pool))
    {}

    /* camera server */
    [[nodiscard]] infrastructure::TcpConnectionType GetConnectionType(tcp::endpoint endpoint) override {
        return infrastructure::TcpConnectionType::CAMERA_CONNECTION;
    }
    [[nodiscard]]  infrastructure::CameraConnectionPayload CreateCameraServerConnection(
        std::shared_ptr<infrastructure::TcpSession> session
    ) override {
        client_is_connected = true;
        return { 0, _buffer_pool };
    };
    void DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> session) override {
        client_is_connected = false;
    }
    [[nodiscard]] bool ClientIsConnected() const {
        return client_is_connected;
    }
    std::shared_ptr<SizedPlaneBufferPool> _buffer_pool;
    std::atomic_bool client_is_connected = false;

    /* dummy for headset server */
    unsigned long CreateHeadsetServerConnection(std::shared_ptr<infrastructure::WritableTcpSession> session) override {
        return 0;
    }
    void DestroyHeadsetServerConnection(std::shared_ptr<infrastructure::WritableTcpSession> session) override {}
};


#endif //AUGMENTEDNORMALCY_TEST_CAMERA_STREAMER_SERVER_HPP

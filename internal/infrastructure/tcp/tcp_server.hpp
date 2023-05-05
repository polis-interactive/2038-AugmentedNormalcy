//
// Created by brucegoose on 3/5/23.
//

#ifndef AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_SERVER_HPP
#define AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_SERVER_HPP

#include <memory>
#include <queue>

#include "tcp_context.hpp"
#include "utils/buffers.hpp"
#include "tcp_packet_header.hpp"

using boost::asio::ip::tcp;
using boost::system::error_code;

typedef boost::asio::ip::address_v4 tcp_addr;

namespace infrastructure {

    enum class TcpConnectionType {
        CAMERA_CONNECTION,
        HEADSET_CONNECTION,
        UNKNOWN_CONNECTION
    };

    class TcpSession {
    public:
        virtual void TryClose(bool is_self_close) = 0;
        virtual tcp_addr GetAddr() = 0;
        virtual unsigned long GetSessionId() = 0;
    };

    class WritableTcpSession: public TcpSession {
    public:
        virtual void Write(std::shared_ptr<SizedBuffer> &&send_buffer) = 0;
    };

    class TcpServerManager {
    public:
        // tcp server
        [[nodiscard]] virtual TcpConnectionType GetConnectionType(tcp_addr endpoint) = 0;
        // camera session
        [[nodiscard]]  virtual unsigned long CreateCameraServerConnection(
            std::shared_ptr<TcpSession> session
        ) = 0;
        virtual void PostCameraServerBuffer(const tcp_addr &addr, std::shared_ptr<ResizableBuffer> &&buffer) = 0;
        virtual void DestroyCameraServerConnection(
                std::shared_ptr<TcpSession> session
        ) = 0;

        // headset session
        [[nodiscard]]  virtual unsigned long CreateHeadsetServerConnection(
            std::shared_ptr<WritableTcpSession> session
        ) = 0;
        virtual void DestroyHeadsetServerConnection(std::shared_ptr<WritableTcpSession> session) = 0;
    };

    class TcpCameraBuffer: public ResizableBuffer {
    public:
        TcpCameraBuffer(std::size_t size, const bool is_leaky):
            _max_size(size), _is_leaky(is_leaky)
        {
            _buffer = new unsigned char[_max_size];
        }
        [[nodiscard]] void * GetMemory() final {
            return _buffer;
        }
        [[nodiscard]] std::size_t GetSize() final {
            return _size;
        }
        virtual void SetSize(std::size_t used_size) {
            _size = used_size;
        };
        [[nodiscard]] bool IsLeakyBuffer() final {
            return _is_leaky;
        };
        ~TcpCameraBuffer() {
            delete[] _buffer;
        }
    private:
        const std::size_t _max_size;
        const bool _is_leaky;
        unsigned char *_buffer;
        std::size_t _size;
    };

    class TcpCameraBufferPool: public std::enable_shared_from_this<TcpCameraBufferPool> {
    public:
        static std::shared_ptr<TcpCameraBufferPool> Create(const int buffer_count, const int buffer_size) {
            auto buffer_pool = std::make_shared<TcpCameraBufferPool>(buffer_count, buffer_size);
            return buffer_pool;
        }
        TcpCameraBufferPool(const int buffer_count, const int buffer_size) {
            for (int i = 0; i < buffer_count; i++) {
                _buffers.push_back(new TcpCameraBuffer(buffer_size, false));
            }
            _leaky_buffer = std::make_shared<TcpCameraBuffer>(buffer_size, true);
        }
        [[nodiscard]] std::shared_ptr<TcpCameraBuffer> GetCameraBuffer() {
            TcpCameraBuffer *buffer = nullptr;
            {
                std::unique_lock<std::mutex> lock(_buffer_mutex);
                if (!_buffers.empty()) {
                    buffer = _buffers.front();
                    _buffers.pop_front();
                }
            }
            if (buffer == nullptr) {
                return _leaky_buffer;
            }
            auto self(shared_from_this());
            auto wrapped_buffer = std::shared_ptr<TcpCameraBuffer>(
                buffer, [this, s = std::move(self)](TcpCameraBuffer * b) mutable {
                    std::unique_lock<std::mutex> lock(_buffer_mutex);
                    _buffers.push_back(b);
                }
            );
            return wrapped_buffer;
        };
        ~TcpCameraBufferPool() {
            TcpCameraBuffer *buffer;
            std::unique_lock<std::mutex> lock(_buffer_mutex);
            while (!_buffers.empty()) {
                buffer = _buffers.front();
                _buffers.pop_front();
                delete buffer;
            }
        }
    private:
        std::shared_ptr<TcpCameraBuffer> _leaky_buffer;
        std::deque<TcpCameraBuffer *> _buffers;
        std::mutex _buffer_mutex;
    };


    class TcpServer;
    class TcpCameraSession : public std::enable_shared_from_this<TcpCameraSession>, public TcpSession {
    public:
        TcpCameraSession() = delete;
        TcpCameraSession (const TcpCameraSession&) = delete;
        TcpCameraSession& operator= (const TcpCameraSession&) = delete;
        // TODO: don't know a better way of doing this. Essentially, TcpServerManager needs to be able to call this
        void TryClose(bool is_self_close) override;
        tcp_addr GetAddr() override {
            return _addr;
        };
        unsigned long GetSessionId() override {
            return _session_id;
        }
    protected:
        friend class TcpServer;
        TcpCameraSession(
            tcp::socket &&socket, std::shared_ptr<TcpServerManager> &_manager,
            const tcp_addr addr, const int read_timeout, const int buffer_count, const int buffer_size
        );
        void Run();
    private:
        void startTimer();
        void readHeader(std::size_t last_bytes);
        void readBody();
        tcp::socket _socket;
        const tcp_addr _addr;
        // TODO: realistically, this should be an underprivileged version of TcpServerManager, but w.e
        std::shared_ptr<TcpServerManager> &_manager;
        unsigned long _session_id= 0;
        boost::asio::deadline_timer _read_timer;
        const int _read_timeout;

        PacketHeader _header;
        std::shared_ptr<TcpCameraBufferPool> _receive_buffer_pool = nullptr;
        std::shared_ptr<TcpCameraBuffer> _receive_buffer = nullptr;
    };

    class TcpHeadsetSession : public std::enable_shared_from_this<TcpHeadsetSession>, public WritableTcpSession {
    public:
        TcpHeadsetSession() = delete;
        TcpHeadsetSession (const TcpHeadsetSession&) = delete;
        TcpHeadsetSession& operator= (const TcpHeadsetSession&) = delete;
        // TODO: don't know a better way of doing this. Essentially, TcpServerManager needs to be able to call this
        void TryClose(bool is_self_close) override;
        tcp_addr GetAddr() override {
            return _addr;
        };
        unsigned long GetSessionId() override {
            return _session_id;
        }
        void Write(std::shared_ptr<SizedBuffer> &&send_buffer) override;
    protected:
        friend class TcpServer;
        TcpHeadsetSession(tcp::socket &&socket, std::shared_ptr<TcpServerManager> &manager, tcp_addr addr);
        void ConnectAndWait();
    private:
        void writeHeader(std::size_t last_bytes);
        void writeBody();
        tcp::socket _socket;
        const tcp_addr _addr;
        // TODO: realistically, this should be an underprivileged version of TcpServerManager, but w.e
        std::shared_ptr<TcpServerManager> &_manager;
        std::atomic<bool> _is_live;
        unsigned long _session_id = 0;
        PacketHeader _header;
        std::mutex _message_mutex;
        std::queue<std::shared_ptr<SizedBuffer>> _message_queue;
    };

    struct TcpServerConfig {
        [[nodiscard]] virtual int get_tcp_server_port() const = 0;
        [[nodiscard]] virtual int get_tcp_server_timeout_on_read() const = 0;
        [[nodiscard]] virtual int get_tcp_camera_session_buffer_count() const = 0;
        [[nodiscard]] virtual int get_tcp_camera_session_buffer_size() const = 0;
    };

    class TcpServer: public std::enable_shared_from_this<TcpServer>{
    public:
        static std::shared_ptr<TcpServer> Create(
            const TcpServerConfig &config, net::io_context &context, std::shared_ptr<TcpServerManager> manager
        ) {
            return std::make_shared<TcpServer>(config, context, std::move(manager));
        }
        TcpServer() = delete;
        TcpServer (const TcpServer&) = delete;
        TcpServer& operator= (const TcpServer&) = delete;
        TcpServer(
            const TcpServerConfig &config, net::io_context &context, std::shared_ptr<TcpServerManager> manager
        );
        void Start();
        void Stop();
    private:
        void acceptConnections();
        std::atomic<bool> _is_stopped = { true };
        net::io_context &_context;
        tcp::acceptor _acceptor;
        tcp::endpoint _endpoint;
        std::shared_ptr<TcpServerManager> _manager;
        const int _read_timeout;
        const int _tcp_camera_session_buffer_count;
        const int _tcp_camera_session_buffer_size;
    };
}

#endif //AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_SERVER_HPP

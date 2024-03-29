//
// Created by brucegoose on 3/5/23.
//

#ifndef AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_SERVER_HPP
#define AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_SERVER_HPP

#include <memory>
#include <queue>

#include "utils/asio_context.hpp"
#include "utils/buffers.hpp"
#include "tcp_utils.hpp"


namespace infrastructure {

    class TcpSession {
    public:
        virtual void TryClose(bool internal_close) = 0;
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
        [[nodiscard]] virtual ConnectionType GetConnectionType(const tcp::endpoint &endpoint) = 0;
        // camera session
        [[nodiscard]]  virtual unsigned long CreateCameraServerConnection(
            std::shared_ptr<TcpSession> &&session
        ) = 0;
        virtual void PostCameraServerBuffer(const tcp_addr &addr, std::shared_ptr<ResizableBuffer> &&buffer) = 0;
        virtual void DestroyCameraServerConnection(
                std::shared_ptr<TcpSession> &&session
        ) = 0;

        // headset session
        [[nodiscard]]  virtual unsigned long CreateHeadsetServerConnection(
            std::shared_ptr<WritableTcpSession> &&session
        ) = 0;
        virtual void DestroyHeadsetServerConnection(std::shared_ptr<WritableTcpSession> &&session) = 0;
    };


    class TcpServer;
    class TcpCameraSession : public std::enable_shared_from_this<TcpCameraSession>, public TcpSession {
    public:
        TcpCameraSession() = delete;
        TcpCameraSession (const TcpCameraSession&) = delete;
        TcpCameraSession& operator= (const TcpCameraSession&) = delete;
        // TODO: don't know a better way of doing this. Essentially, TcpServerManager needs to be able to call this
        void TryClose(bool internal_close) override;
        tcp_addr GetAddr() override {
            return _addr;
        };
        unsigned long GetSessionId() override {
            return _session_id;
        }
        ~TcpCameraSession();
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
        void doClose();
        tcp::socket _socket;
        const tcp_addr _addr;
        // TODO: realistically, this should be an underprivileged version of TcpServerManager, but w.e
        std::shared_ptr<TcpServerManager> &_manager;
        std::atomic<bool> _is_live;
        unsigned long _session_id= 0;
        boost::asio::deadline_timer _read_timer;
        const int _read_timeout;

        PacketHeader _header;
        std::shared_ptr<TcpReadBufferPool> _receive_buffer_pool = nullptr;
        std::shared_ptr<TcpBuffer> _receive_buffer = nullptr;
    };

    class TcpHeadsetSession : public std::enable_shared_from_this<TcpHeadsetSession>, public WritableTcpSession {
    public:
        TcpHeadsetSession() = delete;
        TcpHeadsetSession (const TcpHeadsetSession&) = delete;
        TcpHeadsetSession& operator= (const TcpHeadsetSession&) = delete;
        // TODO: don't know a better way of doing this. Essentially, TcpServerManager needs to be able to call this
        void TryClose(bool internal_close) override;
        tcp_addr GetAddr() override {
            return _addr;
        };
        unsigned long GetSessionId() override {
            return _session_id;
        }
        void Write(std::shared_ptr<SizedBuffer> &&send_buffer) override;
        ~TcpHeadsetSession();
    protected:
        friend class TcpServer;
        TcpHeadsetSession(
            tcp::socket &&socket, std::shared_ptr<TcpServerManager> &manager, tcp_addr addr,
            const int write_timeout, const int buffer_count, const int buffer_size
        );
        void ConnectAndWait();
    private:
        void startTimer();
        void writeHeader(std::size_t last_bytes);
        void writeBody();
        void doClose();
        tcp::socket _socket;
        const tcp_addr _addr;
        // TODO: realistically, this should be an underprivileged version of TcpServerManager, but w.e
        std::shared_ptr<TcpServerManager> &_manager;
        std::atomic<bool> _is_live;
        unsigned long _session_id = 0;

        boost::asio::deadline_timer _write_timer;
        const int _write_timeout;

        PacketHeader _header;
        std::mutex _message_mutex;
        std::shared_ptr<TcpWriteBufferPool> _copy_buffer_pool;
        std::queue<std::shared_ptr<SizedBuffer>> _message_queue;
    };

    struct TcpServerConfig {
        [[nodiscard]] virtual int get_tcp_server_port() const = 0;
        [[nodiscard]] virtual int get_tcp_server_timeout() const = 0;
        [[nodiscard]] virtual int get_tcp_camera_session_buffer_count() const = 0;
        [[nodiscard]] virtual int get_tcp_headset_session_buffer_count() const = 0;
        [[nodiscard]] virtual int get_tcp_server_buffer_size() const = 0;
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
        ~TcpServer();
    private:
        void acceptConnections();
        std::atomic<bool> _is_stopped = { true };
        net::io_context &_context;
        tcp::acceptor _acceptor;
        tcp::endpoint _endpoint;
        std::shared_ptr<TcpServerManager> _manager;
        const int _read_write_timeout;
        const int _tcp_camera_session_buffer_count;
        const int _tcp_headset_session_buffer_count;
        const int _tcp_session_buffer_size;
    };
}

#endif //AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_SERVER_HPP

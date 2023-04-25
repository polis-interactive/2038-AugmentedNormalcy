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


namespace infrastructure {

    enum class TcpConnectionType {
        CAMERA_CONNECTION,
        HEADSET_CONNECTION,
        UNKNOWN_CONNECTION
    };

    typedef std::pair<unsigned long, std::shared_ptr<SizedPlaneBufferPool>> CameraConnectionPayload;

    class TcpSession {
    public:
        virtual void TryClose(bool is_self_close) = 0;
        virtual tcp::endpoint GetEndpoint() = 0;
        virtual unsigned long GetSessionId() = 0;
    };

    class WritableTcpSession: public TcpSession {
    public:
        virtual void Write(std::shared_ptr<SizedBuffer> &&send_buffer) = 0;
    };

    class TcpServerManager {
    public:
        // tcp server
        [[nodiscard]] virtual TcpConnectionType GetConnectionType(tcp::endpoint endpoint) = 0;
        // camera session
        [[nodiscard]]  virtual CameraConnectionPayload CreateCameraServerConnection(
            std::shared_ptr<TcpSession> session
        ) = 0;
        virtual void DestroyCameraServerConnection(
                std::shared_ptr<TcpSession> session
        ) = 0;

        // headset session
        [[nodiscard]]  virtual unsigned long CreateHeadsetServerConnection(
            std::shared_ptr<WritableTcpSession> session
        ) = 0;
        virtual void DestroyHeadsetServerConnection(std::shared_ptr<WritableTcpSession> session) = 0;
    };


    class TcpServer;
    class TcpCameraSession : public std::enable_shared_from_this<TcpCameraSession>, public TcpSession {
    public:
        TcpCameraSession() = delete;
        TcpCameraSession (const TcpCameraSession&) = delete;
        TcpCameraSession& operator= (const TcpCameraSession&) = delete;
        // TODO: don't know a better way of doing this. Essentially, TcpServerManager needs to be able to call this
        void TryClose(bool is_self_close) override;
        tcp::endpoint GetEndpoint() override {
            return _socket.remote_endpoint();
        };
        unsigned long GetSessionId() override {
            return _session_id;
        }
    protected:
        friend class TcpServer;
        TcpCameraSession(tcp::socket &&socket, std::shared_ptr<TcpServerManager> &_manager);
        void Run();
    private:
        void readHeader(std::size_t last_bytes);
        void readBody();
        tcp::socket _socket;
        // TODO: realistically, this should be an underprivileged version of TcpServerManager, but w.e
        std::shared_ptr<TcpServerManager> &_manager;
        std::shared_ptr<SizedPlaneBufferPool> _plane_buffer_pool = nullptr;
        unsigned long _session_id= -1;

        PacketHeader _header;
        std::shared_ptr<SizedBufferPool> _plane_buffer = nullptr;
        std::shared_ptr<SizedBuffer> _buffer = nullptr;
    };

    class TcpHeadsetSession : public std::enable_shared_from_this<TcpHeadsetSession>, public WritableTcpSession {
    public:
        TcpHeadsetSession() = delete;
        TcpHeadsetSession (const TcpHeadsetSession&) = delete;
        TcpHeadsetSession& operator= (const TcpHeadsetSession&) = delete;
        // TODO: don't know a better way of doing this. Essentially, TcpServerManager needs to be able to call this
        void TryClose(bool is_self_close) override;
        tcp::endpoint GetEndpoint() override {
            return _socket.remote_endpoint();
        };
        unsigned long GetSessionId() override {
            return _session_id;
        }
        void Write(std::shared_ptr<SizedBuffer> &&send_buffer) override;
    protected:
        friend class TcpServer;
        TcpHeadsetSession(tcp::socket &&socket, std::shared_ptr<TcpServerManager> &manager);
        void ConnectAndWait();
    private:
        void writeHeader(std::size_t last_bytes);
        void writeBody();
        tcp::socket _socket;
        // TODO: realistically, this should be an underprivileged version of TcpServerManager, but w.e
        std::shared_ptr<TcpServerManager> &_manager;
        std::atomic<bool> _is_live;
        unsigned long _session_id = -1;
        PacketHeader _header;
        std::mutex _message_mutex;
        std::queue<std::shared_ptr<SizedBuffer>> _message_queue;
    };

    struct TcpServerConfig {
        [[nodiscard]] virtual int get_tcp_server_port() const = 0;
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
    };
}

#endif //AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_SERVER_HPP

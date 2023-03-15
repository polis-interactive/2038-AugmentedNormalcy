//
// Created by brucegoose on 3/5/23.
//

#ifndef AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_SERVER_HPP
#define AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_SERVER_HPP

#include <boost/asio.hpp>
#include <map>
#include "tcp_context.hpp"

using boost::asio::ip::tcp;
using boost::system::error_code;

struct SizedBuffer {
    [[nodiscard]] virtual void *GetMemory() = 0;
    [[nodiscard]] virtual std::size_t GetSize() = 0;
};

class CameraBufferPool {
public:
    [[nodiscard]] virtual std::shared_ptr<SizedBuffer> &&GetCameraBuffer() = 0;
    virtual void SendCameraBuffer(std::shared_ptr<SizedBuffer> &&buffer) = 0;
};

namespace infrastructure {

    enum class TcpConnectionType {
        CAMERA_CONNECTION,
        HEADSET_CONNECTION,
        UNKNOWN_CONNECTION
    };

    typedef std::pair<unsigned long, std::shared_ptr<CameraBufferPool>> CameraConnectionPayload;
    typedef std::function<void(std::shared_ptr<SizedBuffer>&&)> HeadsetWriteCall;

    class TcpServerManager {
    public:
        // tcp server
        [[nodiscard]] virtual TcpConnectionType GetConnectionType(tcp::endpoint endpoint) = 0;
        // camera session
        [[nodiscard]]  virtual CameraConnectionPayload CreateCameraConnection(tcp::endpoint endpoint) = 0;
        virtual void DestroyCameraConnection(tcp::endpoint endpoint, unsigned long session_id) = 0;

        // headset session
        [[nodiscard]]  virtual unsigned long CreateHeadsetConnection(
            tcp::endpoint endpoint,
            HeadsetWriteCall writeCall
        ) = 0;
        virtual void DestroyHeadsetConnection(tcp::endpoint endpoint, unsigned long session_id) = 0;
    };


    class TcpServer;
    class TcpCameraSession : public std::enable_shared_from_this<TcpCameraSession> {
    public:
        TcpCameraSession() = delete;
        TcpCameraSession (const TcpCameraSession&) = delete;
        TcpCameraSession& operator= (const TcpCameraSession&) = delete;
        // TODO: don't know a better way of doing this. Essentially, TcpServerManager needs to be able to call this
        void TryClose();
    protected:
        friend class TcpServer;
        TcpCameraSession(tcp::socket &&socket, std::shared_ptr<TcpServerManager> &_manager);
        void Run();
    private:
        void readStream();
        tcp::socket _socket;
        // TODO: realistically, this should be an underprivileged version of TcpServerManager, but w.e
        std::shared_ptr<TcpServerManager> &_manager;
        std::shared_ptr<CameraBufferPool> _buffer_pool = nullptr;
        unsigned long _session_id;
    };

    class TcpHeadsetSession : public std::enable_shared_from_this<TcpHeadsetSession> {
    public:
        TcpHeadsetSession() = delete;
        TcpHeadsetSession (const TcpHeadsetSession&) = delete;
        TcpHeadsetSession& operator= (const TcpHeadsetSession&) = delete;
        // TODO: don't know a better way of doing this. Essentially, TcpServerManager needs to be able to call this
        void TryClose();
    protected:
        friend class TcpServer;
        TcpHeadsetSession(tcp::socket &&socket, std::shared_ptr<TcpServerManager> &manager);
    private:
        void write(std::shared_ptr<SizedBuffer> &&send_buffer);
        tcp::socket _socket;
        // TODO: realistically, this should be an underprivileged version of TcpServerManager, but w.e
        std::shared_ptr<TcpServerManager> &_manager;
        std::atomic<bool> _is_live;
        unsigned long _session_id;
    };

    struct TcpServerConfig {
        [[nodiscard]] virtual int get_tcp_server_port() const = 0;
    };

    class TcpServer {
    public:
        TcpServer() = delete;
        TcpServer (const TcpServer&) = delete;
        TcpServer& operator= (const TcpServer&) = delete;
        TcpServer(
            const TcpServerConfig &config, net::io_context &context, std::shared_ptr<TcpServerManager> &manager
        );
        void Start();
        void Stop();
    private:
        void acceptConnections();
        std::atomic<bool> _is_stopped = { true };
        net::io_context &_context;
        tcp::acceptor _acceptor;
        tcp::endpoint _endpoint;
        std::shared_ptr<TcpServerManager> &_manager;
    };
}

#endif //AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_SERVER_HPP

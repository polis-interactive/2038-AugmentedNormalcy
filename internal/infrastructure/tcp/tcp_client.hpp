//
// Created by brucegoose on 3/16/23.
//

#ifndef AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_CLIENT_HPP
#define AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_CLIENT_HPP

#include <utility>
#include <string>

#include "tcp_common.hpp"
#include "tcp_context.hpp"

using boost::asio::ip::tcp;
using boost::system::error_code;

namespace infrastructure {

    struct TcpClientConfig {
        [[nodiscard]] virtual std::string get_tcp_server_host() const = 0;
        [[nodiscard]] virtual int get_tcp_server_port() const = 0;
        [[nodiscard]] virtual bool get_tcp_client_is_camera() const = 0;
    };

    typedef std::function<void(std::shared_ptr<SizedBuffer>&&)> CameraWriteCall;

    class TcpClientManager {
    public:
        // camera session
        virtual void CreateCameraConnection(CameraWriteCall writeCall) = 0;
        virtual void DestroyCameraConnection() = 0;

        // headset session
        virtual std::shared_ptr<PushingBufferPool> CreateHeadsetConnection() = 0;
        virtual void DestroyHeadsetConnection() = 0;
    };

    class TcpClient {
    public:
        TcpClient() = delete;
        TcpClient (const TcpClient&) = delete;
        TcpClient& operator= (const TcpClient&) = delete;
        TcpClient (
            const TcpClientConfig &config, net::io_context &context, std::shared_ptr<TcpClientManager> &manager
        );
        void Start();
        void Stop();
    private:
        void startConnection(bool is_initial_connection);
        void startWrite();
        void write(std::shared_ptr<SizedBuffer> &&buffer);
        void startRead();
        void read();
        void disconnect(error_code ec);
        void reconnect(error_code ec);
        const bool _is_camera;
        std::atomic<bool> _is_stopped = {true};
        std::atomic<bool> _is_connected = {false};
        net::io_context &_context;
        tcp::endpoint _remote_endpoint;
        tcp::socket _socket;
        std::shared_ptr<TcpClientManager> &_manager;
        std::shared_ptr<PushingBufferPool> _buffer_pool = nullptr;
    };

}

#endif //AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_CLIENT_HPP

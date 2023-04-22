//
// Created by brucegoose on 3/16/23.
//

#ifndef AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_CLIENT_HPP
#define AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_CLIENT_HPP

#include <utility>
#include <string>
#include <memory>
#include <queue>

#include "utils/buffers.hpp"
#include "tcp_context.hpp"

using boost::asio::ip::tcp;
using boost::system::error_code;

namespace infrastructure {

    struct TcpClientConfig {
        [[nodiscard]] virtual std::string get_tcp_server_host() const = 0;
        [[nodiscard]] virtual int get_tcp_server_port() const = 0;
        [[nodiscard]] virtual bool get_tcp_client_is_camera() const = 0;
    };


    class TcpClientManager {
    public:
        // camera session
        virtual void CreateCameraClientConnection() = 0;
        virtual void DestroyCameraClientConnection() = 0;

        // headset session
        virtual std::shared_ptr<ResizableBufferPool> CreateHeadsetClientConnection() = 0;
        virtual void DestroyHeadsetClientConnection() = 0;
    };

    class TcpClient: public std::enable_shared_from_this<TcpClient> {
    public:
        static std::shared_ptr<TcpClient> Create(
            const TcpClientConfig &config, net::io_context &context, std::shared_ptr<TcpClientManager> manager
        ) {
            return std::make_shared<TcpClient>(config, context, std::move(manager));
        }
        TcpClient() = delete;
        TcpClient (const TcpClient&) = delete;
        TcpClient& operator= (const TcpClient&) = delete;
        ~TcpClient() { Stop(); }
        TcpClient (
            const TcpClientConfig &config, net::io_context &context, std::shared_ptr<TcpClientManager> manager
        );
        void Start();
        void Stop();
        void Post(std::shared_ptr<SizedBuffer> &&buffer);
    private:
        void startConnection(bool is_initial_connection);
        void startWrite();
        void startRead();
        void readHeader();
        void readBody();
        void disconnect(error_code ec);
        void reconnect(error_code ec);
        const bool _is_camera;
        std::atomic<bool> _is_stopped = {true};
        std::atomic<bool> _is_connected = {false};
        net::io_context &_context;
        tcp::endpoint _remote_endpoint;
        std::shared_ptr<tcp::socket> _socket = nullptr;
        std::shared_ptr<TcpClientManager> _manager;
        std::shared_ptr<ResizableBufferPool> _buffer_pool = nullptr;

        TcpReaderMessage _header;
    };

}

#endif //AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_CLIENT_HPP

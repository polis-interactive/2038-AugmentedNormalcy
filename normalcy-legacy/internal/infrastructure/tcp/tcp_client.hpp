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
#include "utils/asio_context.hpp"
#include "tcp_utils.hpp"


namespace infrastructure {

    struct TcpClientConfig {
        [[nodiscard]] virtual std::string get_tcp_server_host() const = 0;
        [[nodiscard]] virtual int get_tcp_server_port() const = 0;
        [[nodiscard]] virtual ConnectionType get_tcp_client_connection_type() const = 0;
        [[nodiscard]] virtual bool get_tcp_client_used_fixed_port() const = 0;
        [[nodiscard]] virtual int get_tcp_client_timeout_on_read() const = 0;
        [[nodiscard]] virtual int get_tcp_client_read_buffer_count() const = 0;
        [[nodiscard]] virtual int get_tcp_client_read_buffer_size() const = 0;
    };


    class TcpClientManager {
    public:
        // camera session
        virtual void CreateCameraClientConnection() = 0;
        virtual void DestroyCameraClientConnection() = 0;

        // headset session
        virtual void CreateHeadsetClientConnection() = 0;
        virtual void PostHeadsetClientBuffer(std::shared_ptr<SizedBuffer> &&buffer) = 0;
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
        TcpClient (
            const TcpClientConfig &config, net::io_context &context, std::shared_ptr<TcpClientManager> manager
        );
        ~TcpClient();
        void Start();
        void Stop();
        void Post(std::shared_ptr<SizedBuffer> &&buffer);
    private:
        void startConnection(bool is_initial_connection);
        void startWrite();
        void writeHeader(std::size_t last_bytes);
        void writeBody();
        void startRead();
        void startTimer();
        void readHeader(std::size_t last_bytes);
        void readBody();
        void disconnect(error_code ec);
        void reconnect(error_code ec);
        const ConnectionType _connection_type;
        std::atomic<bool> _is_stopped = {true};
        std::atomic<bool> _is_connected = {false};
        tcp::endpoint _remote_endpoint;
        std::shared_ptr<tcp::socket> _socket = nullptr;
        std::shared_ptr<TcpClientManager> _manager;

        net::deadline_timer _read_timer;
        const int _read_timeout;
        const bool _use_fixed_port;

        PacketHeader _header;

        std::mutex _send_buffer_mutex;
        std::queue<std::shared_ptr<SizedBuffer>> _send_buffer_queue = {};

        std::shared_ptr<TcpReadBufferPool> _receive_buffer_pool = nullptr;
        std::shared_ptr<TcpBuffer> _receive_buffer = nullptr;
    };

}

#endif //AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_CLIENT_HPP

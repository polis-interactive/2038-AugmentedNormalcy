//
// Created by brucegoose on 4/8/23.
//

#ifndef AUGMENTEDNORMALCY_SERVICE_SERVER_ENCODER_HPP
#define AUGMENTEDNORMALCY_SERVICE_SERVER_ENCODER_HPP

#include <boost/asio/ip/tcp.hpp>

#include "infrastructure/tcp/tcp_context.hpp"
#include "infrastructure/tcp/tcp_server.hpp"
#include "infrastructure/encoder/jetson_encoder.hpp"

using boost::asio::ip::tcp;

namespace service {
    struct ServerEncoderConfig:
        public infrastructure::TcpContextConfig,
        public infrastructure::TcpServerConfig,
        public infrastructure::EncoderConfig
    {
        ServerEncoderConfig(
            int tcp_pool_size, int tcp_server_port, int upstream_buffer_count, int downstream_buffer_count
        ):
            _tcp_pool_size(tcp_pool_size),
            _tcp_server_port(tcp_server_port),
            _upstream_buffer_count(upstream_buffer_count),
            _downstream_buffer_count(downstream_buffer_count)
        {}
        [[nodiscard]] int get_tcp_pool_size() const override {
            return _tcp_pool_size;
        };
        [[nodiscard]] int get_tcp_server_port() const override {
            return _tcp_server_port;
        }
        [[nodiscard]] unsigned int get_encoder_upstream_buffer_count() const override {
            return _upstream_buffer_count;
        }
        [[nodiscard]] unsigned int get_encoder_downstream_buffer_count() const override {
            return _downstream_buffer_count;
        }
        [[nodiscard]] std::pair<int, int> get_encoder_width_height() const override {
            return { 1536, 864 };
        }
    private:
        const int _tcp_pool_size;
        const int _tcp_server_port;
        const int _upstream_buffer_count;
        const int _downstream_buffer_count;
    };

    class ServerEncoder:
        public std::enable_shared_from_this<ServerEncoder>,
        public infrastructure::TcpServerManager
    {
    public:
        static std::shared_ptr<ServerEncoder> Create(const ServerEncoderConfig &config);
        ServerEncoder(): _is_started(false) {}
        void Start() {
            if (_is_started) {
                return;
            }
            _tcp_context->Start();
            _tcp_server->Start();
            _is_started = true;
        }
        void Stop() {
            if (!_is_started) {
                return;
            }
            _tcp_server->Stop();
            _tcp_context->Stop();
            _is_started = false;
        }
        // tcp server
        [[nodiscard]] infrastructure::TcpConnectionType GetConnectionType(tcp::endpoint endpoint);
        // camera session
        [[nodiscard]]  infrastructure::CameraConnectionPayload CreateCameraServerConnection(tcp::endpoint endpoint);
        void DestroyCameraServerConnection(tcp::endpoint endpoint, unsigned long session_id);

        // headset session
        [[nodiscard]] unsigned long CreateHeadsetServerConnection(
            tcp::endpoint endpoint,
            std::shared_ptr<infrastructure::WritableTcpSession> session
        );
        void DestroyHeadsetServerConnection(tcp::endpoint endpoint, unsigned long session_id);
    private:
        void initialize(const ServerEncoderConfig &config);
        std::atomic_bool _is_started = false;
        std::shared_ptr<infrastructure::TcpContext> _tcp_context = nullptr;
        std::shared_ptr<infrastructure::TcpServer> _tcp_server = nullptr;
        std::shared_ptr<infrastructure::TcpSession> _camera_session = nullptr;
        std::mutex _camera_mutex;
        std::shared_ptr<infrastructure::WritableTcpSession> _headset_session = nullptr;
        std::mutex _headset_mute;
    };
}

#endif //AUGMENTEDNORMALCY_SERVICE_SERVER_ENCODER_HPP

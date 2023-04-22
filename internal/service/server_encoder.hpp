//
// Created by brucegoose on 4/8/23.
//

#ifndef AUGMENTEDNORMALCY_SERVICE_SERVER_ENCODER_HPP
#define AUGMENTEDNORMALCY_SERVICE_SERVER_ENCODER_HPP

#include <boost/asio/ip/tcp.hpp>
#include <utility>

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
        explicit ServerEncoder(ServerEncoderConfig config): _is_started(false), _conf(std::move(config)) {}
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
            {
                std::unique_lock<std::mutex> lock(_camera_mutex);
                if (_camera_session) {
                    _camera_session->TryClose();
                    _camera_session.reset();
                }
            }
            {
                std::unique_lock<std::mutex> lock(_headset_mutex);
                if (_headset_session) {
                    _headset_session->TryClose();
                    _headset_session.reset();
                }
            }
            _tcp_server->Stop();
            _tcp_context->Stop();
            _is_started = false;
        }
        void Unset() {
            _tcp_server.reset();
            _tcp_context.reset();
        }
        // tcp server
        [[nodiscard]] infrastructure::TcpConnectionType GetConnectionType(tcp::endpoint endpoint) override;
        // camera session
        [[nodiscard]]  infrastructure::CameraConnectionPayload CreateCameraServerConnection(
            std::shared_ptr<infrastructure::TcpSession> camera_session
        ) override;
        void DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> camera_session) override;

        // headset session
        [[nodiscard]] unsigned long CreateHeadsetServerConnection(
            std::shared_ptr<infrastructure::WritableTcpSession> headset_session
        ) override;
        void DestroyHeadsetServerConnection(
            std::shared_ptr<infrastructure::WritableTcpSession> headset_session
        ) override;
    private:
        void initialize();
        ServerEncoderConfig _conf;
        std::atomic_bool _is_started = false;
        std::shared_ptr<infrastructure::TcpContext> _tcp_context = nullptr;
        std::shared_ptr<infrastructure::TcpServer> _tcp_server = nullptr;
        std::shared_ptr<infrastructure::TcpSession> _camera_session = nullptr;
        std::mutex _camera_mutex;
        std::shared_ptr<infrastructure::WritableTcpSession> _headset_session = nullptr;
        std::mutex _headset_mutex;
        std::atomic<unsigned long> _last_session_number = { 0 };
    };
}

#endif //AUGMENTEDNORMALCY_SERVICE_SERVER_ENCODER_HPP

//
// Created by brucegoose on 3/20/23.
//

#include <utility>

#include "infrastructure/tcp/tcp_context.hpp"
#include "infrastructure/tcp/tcp_client.hpp"
#include "infrastructure/camera/camera.hpp"

namespace service {
    struct CameraStreamerConfig:
        public infrastructure::TcpContextConfig,
        public infrastructure::TcpClientConfig,
        public infrastructure::CameraConfig
    {
        CameraStreamerConfig(int tcp_pool_size, std::string tcp_server_host, int tcp_server_port):
            _tcp_server_host(std::move(tcp_server_host)),
            _tcp_server_port(tcp_server_port)
        {}
        [[nodiscard]] int get_tcp_pool_size() const override {
            return 2;
        }
        [[nodiscard]] std::string get_tcp_server_host() const override {
            return _tcp_server_host;
        }
        [[nodiscard]] int get_tcp_server_port() const override {
            return _tcp_server_port;
        }
        [[nodiscard]] bool get_tcp_client_is_camera() const override {
            return true;
        }
    private:
        const std::string _tcp_server_host;
        const int _tcp_server_port;
    };

    class CameraStreamer {
        CameraStreamer(const CameraStreamerConfig &config)
        {
            _tcp_context = infrastructure::TcpContext::Create(config);
            // need to write with all callbacks and what not... for _camera and _tcp_client
        }
        void start() {
            if (is_started) {
                return;
            }
            _camera->Start();
            _tcp_context->Start();
            _tcp_client->Start();
            is_started = true;
        }
        void Stop() {
            if (!is_started) {
                return;
            }
            _tcp_client->Stop();
            _tcp_context->Stop();
            _camera->Stop();
            is_started = false;
        }
    private:
        std::atomic_bool is_started = false;
        std::shared_ptr<infrastructure::Camera> _camera = nullptr;
        std::shared_ptr<infrastructure::TcpContext> _tcp_context = nullptr;
        std::shared_ptr<infrastructure::TcpClient> _tcp_client = nullptr;
    };
}
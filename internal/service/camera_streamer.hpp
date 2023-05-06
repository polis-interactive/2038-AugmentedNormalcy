//
// Created by brucegoose on 3/20/23.
//

#ifndef AUGMENTEDNORMALCY_SERVICE_CAMERA_STREAMER_HPP
#define AUGMENTEDNORMALCY_SERVICE_CAMERA_STREAMER_HPP

#include <utility>

#include "infrastructure/tcp/tcp_context.hpp"
#include "infrastructure/tcp/tcp_client.hpp"
#include "infrastructure/camera/libcamera_camera.hpp"
#include "infrastructure/encoder/sw_encoder.hpp"

namespace service {
    struct CameraStreamerConfig:
        public infrastructure::TcpContextConfig,
        public infrastructure::TcpClientConfig,
        public infrastructure::LibcameraConfig,
        public infrastructure::EncoderConfig
    {
        CameraStreamerConfig(
            std::string tcp_server_host, int tcp_server_port,
            std::pair<int, int> camera_width_height,
            int encoder_buffers_downstream
        ):
            _tcp_server_host(std::move(tcp_server_host)),
            _tcp_server_port(tcp_server_port),
            _camera_width_height(std::move(camera_width_height)),
            _encoder_buffers_downstream(encoder_buffers_downstream)
        {}
        [[nodiscard]] int get_tcp_pool_size() const override {
            return 1;
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
        [[nodiscard]] std::pair<int, int> get_camera_width_height() const override {
            return _camera_width_height;
        };
        [[nodiscard]] int get_fps() const override {
            return 30;
        };
        [[nodiscard]] int get_camera_buffer_count() const override {
            return 5;
        };
        [[nodiscard]] int get_tcp_client_timeout_on_read() const override {
            return 5;
        };
        [[nodiscard]] unsigned int get_encoder_downstream_buffer_count() const override {
            return _encoder_buffers_downstream;
        };
        [[nodiscard]] std::pair<int, int> get_encoder_width_height() const override {
            return _camera_width_height;
        };
    private:
        const std::string _tcp_server_host;
        const int _tcp_server_port;
        const std::pair<int, int> _camera_width_height;
        const int _encoder_buffers_downstream;
    };

    class CameraStreamer:
            public std::enable_shared_from_this<CameraStreamer>,
            public infrastructure::TcpClientManager
    {
    public:
        static std::shared_ptr<CameraStreamer> Create(const CameraStreamerConfig &config);
        CameraStreamer(): _is_started(false) {}
        void Start() {
            if (_is_started) {
                return;
            }
            _camera->Start();
            _encoder->Start();
            _tcp_context->Start();
            _tcp_client->Start();
            _is_started = true;
        }
        void Stop() {
            if (!_is_started) {
                return;
            }
            _tcp_client->Stop();
            _tcp_context->Stop();
            _encoder->Stop();
            _camera->Stop();
            _is_started = false;
        }
        void Unset() {
            _tcp_client.reset();
            _tcp_context.reset();
            _camera.reset();
        }
        // currently, we are just trying to get this thing streaming; we don't care about
        // holding sessions client side
        void CreateCameraClientConnection() override {};
        void DestroyCameraClientConnection() override {};
        std::shared_ptr<ResizableBufferPool> CreateHeadsetClientConnection() override { return nullptr; };
        void DestroyHeadsetClientConnection() override {};
    private:
        void initialize(const CameraStreamerConfig &config);
        std::atomic_bool _is_started = false;
        std::shared_ptr<infrastructure::LibcameraCamera> _camera = nullptr;
        std::shared_ptr<infrastructure::SwEncoder> _encoder = nullptr;
        std::shared_ptr<infrastructure::TcpContext> _tcp_context = nullptr;
        std::shared_ptr<infrastructure::TcpClient> _tcp_client = nullptr;
    };
}

#endif //AUGMENTEDNORMALCY_SERVICE_CAMERA_STREAMER_HPP
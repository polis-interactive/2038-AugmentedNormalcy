//
// Created by brucegoose on 3/20/23.
//

#ifndef SERVICE_CAMERA_STREAMER_HPP
#define SERVICE_CAMERA_STREAMER_HPP

#include <utility>

#include "utils/asio_context.hpp"
#include "infrastructure/tcp/tcp_client.hpp"
#include "infrastructure/camera/camera.hpp"
#include "infrastructure/encoder/encoder.hpp"

namespace service {
    struct CameraStreamerConfig:
        public AsioContextConfig,
        public infrastructure::TcpClientConfig,
        public infrastructure::CameraConfig,
        public infrastructure::EncoderConfig
    {
        CameraStreamerConfig(
            std::string tcp_server_host, int tcp_server_port,
            bool tcp_client_use_fixed_port,
            infrastructure::CameraType camera_type,
            std::pair<int, int> camera_width_height,
            float camera_lens_position,
            float camera_frames_per_second,
            infrastructure::EncoderType encoder_type,
            int encoder_buffers_downstream
        ):
            _tcp_server_host(std::move(tcp_server_host)),
            _tcp_server_port(tcp_server_port),
            _tcp_client_use_fixed_port(tcp_client_use_fixed_port),
            _camera_type(camera_type),
            _camera_width_height(std::move(camera_width_height)),
            _camera_lens_position(camera_lens_position),
            _camera_frames_per_second(camera_frames_per_second),
            _encoder_type(encoder_type),
            _encoder_buffers_downstream(encoder_buffers_downstream)
        {}
        [[nodiscard]] int get_asio_pool_size() const override {
            return 1;
        }
        [[nodiscard]] std::string get_tcp_server_host() const override {
            return _tcp_server_host;
        }
        [[nodiscard]] int get_tcp_server_port() const override {
            return _tcp_server_port;
        }
        [[nodiscard]] ConnectionType get_tcp_client_connection_type() const override {
            return ConnectionType::CAMERA_CONNECTION;
        }
        [[nodiscard]] bool get_tcp_client_used_fixed_port() const override {
            return _tcp_client_use_fixed_port;
        }
        [[nodiscard]] infrastructure::CameraType get_camera_type() const override {
            return _camera_type;
        };
        [[nodiscard]] std::pair<int, int> get_camera_width_height() const override {
            return _camera_width_height;
        };
        [[nodiscard]] float get_lens_position() const override {
            return _camera_lens_position;
        };
        [[nodiscard]] float get_fps() const override {
            return _camera_frames_per_second;
        };
        [[nodiscard]] int get_camera_buffer_count() const override {
            return 5;
        };
        [[nodiscard]] int get_tcp_client_timeout_on_read() const override {
            return 5;
        };
        [[nodiscard]] infrastructure::EncoderType get_encoder_type() const override {
            return _encoder_type;
        };
        [[nodiscard]] unsigned int get_encoder_downstream_buffer_count() const override {
            return _encoder_buffers_downstream;
        };
        [[nodiscard]] std::pair<int, int> get_encoder_width_height() const override {
            return _camera_width_height;
        };
        /* not use but meh */
        [[nodiscard]] int get_tcp_client_read_buffer_count() const override {
            return _encoder_buffers_downstream;
        };
        [[nodiscard]] int get_tcp_client_read_buffer_size() const override {
            return _camera_width_height.first * _camera_width_height.second * 3 / 2;
        };
    private:
        const std::string _tcp_server_host;
        const int _tcp_server_port;
        const bool _tcp_client_use_fixed_port;
        const infrastructure::CameraType _camera_type;
        const std::pair<int, int> _camera_width_height;
        const float _camera_lens_position;
        const float _camera_frames_per_second;
        const infrastructure::EncoderType _encoder_type;
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
            _asio_context->Start();
            _tcp_client->Start();
            _is_started = true;
        }
        void Stop() {
            if (!_is_started) {
                return;
            }
            _tcp_client->Stop();
            _asio_context->Stop();
            _encoder->Stop();
            _camera->Stop();
            _is_started = false;
        }
        void Unset() {
            _tcp_client.reset();
            _asio_context.reset();
            _camera.reset();
        }
        // currently, we are just trying to get this thing streaming; we don't care about
        // holding sessions client side
        void CreateCameraClientConnection() override {};
        void DestroyCameraClientConnection() override {};
        void CreateHeadsetClientConnection() override {};
        void PostHeadsetClientBuffer(std::shared_ptr<SizedBuffer> &&buffer) override {};
        void DestroyHeadsetClientConnection() override {};
    private:
        void initialize(const CameraStreamerConfig &config);
        std::atomic_bool _is_started = false;
        std::shared_ptr<infrastructure::Camera> _camera = nullptr;
        std::shared_ptr<infrastructure::Encoder> _encoder = nullptr;
        std::shared_ptr<AsioContext> _asio_context = nullptr;
        std::shared_ptr<infrastructure::TcpClient> _tcp_client = nullptr;
    };
}

#endif //SERVICE_CAMERA_STREAMER_HPP
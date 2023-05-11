//
// Created by brucegoose on 4/15/23.
//

#ifndef SERVICE_HEADSET_STREAMER_HPP
#define SERVICE_HEADSET_STREAMER_HPP

#include <utility>

#include "infrastructure/tcp/tcp_context.hpp"
#include "infrastructure/tcp/tcp_client.hpp"
#include "infrastructure/decoder/sw_decoder.hpp"
#include "infrastructure/graphics/graphics.hpp"

namespace service {
    struct HeadsetStreamerConfig:
        public infrastructure::TcpContextConfig,
        public infrastructure::TcpClientConfig,
        public infrastructure::DecoderConfig,
        public infrastructure::GraphicsConfig
    {
        HeadsetStreamerConfig(
                std::string tcp_server_host, int tcp_server_port, std::pair<int, int> image_width_height,
                int tcp_read_buffers, int decoder_buffers_downstream
        ):
            _tcp_server_host(std::move(tcp_server_host)),
            _tcp_server_port(tcp_server_port),
            _tcp_read_buffers(tcp_read_buffers),
            _decoder_buffers_downstream(decoder_buffers_downstream),
            _image_width_height(std::move(image_width_height))
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
            return false;
        }
        [[nodiscard]] std::pair<int, int> get_image_width_height() const override {
            return _image_width_height;
        };
        [[nodiscard]] unsigned int get_decoder_downstream_buffer_count() const override {
            return _decoder_buffers_downstream;
        };
        [[nodiscard]] std::pair<int, int> get_decoder_width_height() const override {
            return _image_width_height;
        };
        [[nodiscard]] int get_tcp_client_timeout_on_read() const override {
            return 1;
        };
        [[nodiscard]] int get_tcp_client_read_buffer_count() const override {
            return _tcp_read_buffers;
        };
        [[nodiscard]] int get_tcp_client_read_buffer_size() const override {
            return _image_width_height.first * _image_width_height.second * 3 / 2;
        };
    private:
        const std::string _tcp_server_host;
        const int _tcp_server_port;
        const int _tcp_read_buffers;
        const int _decoder_buffers_downstream;
        const std::pair<int, int> _image_width_height;
    };

    class HeadsetStreamer:
        public std::enable_shared_from_this<HeadsetStreamer>,
        public infrastructure::TcpClientManager
    {
    public:
        static std::shared_ptr<HeadsetStreamer> Create(const HeadsetStreamerConfig &config);
        HeadsetStreamer(): _is_started(false) {}
        void Start() {
            if (_is_started) {
                return;
            }
            _graphics->Start();
            _decoder->Start();
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
            _graphics->Stop();
            _decoder->Stop();
            _is_started = false;
        }
        void Unset() {
            _tcp_client.reset();
            _tcp_context.reset();
            _graphics.reset();
            _decoder.reset();
        }
        // camera isn't an option so no need to initialize it; don't need to hold state, just give whoever asks the
        // decoder; in the future, we'll make sure only one person can "have it at a time"
        void CreateCameraClientConnection() override {};
        void DestroyCameraClientConnection() override {};
        void CreateHeadsetClientConnection() override {};
        void PostHeadsetClientBuffer(std::shared_ptr<SizedBuffer> &&buffer) override;
        void DestroyHeadsetClientConnection() override {};
    private:
        void initialize(const HeadsetStreamerConfig &config);
        std::atomic_bool _is_started = false;
        std::shared_ptr<infrastructure::Graphics> _graphics = nullptr;
        std::shared_ptr<infrastructure::SwDecoder> _decoder = nullptr;
        std::shared_ptr<infrastructure::TcpContext> _tcp_context = nullptr;
        std::shared_ptr<infrastructure::TcpClient> _tcp_client = nullptr;
    };
}

#endif //SERVICE_HEADSET_STREAMER_HPP

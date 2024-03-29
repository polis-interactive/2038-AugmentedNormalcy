//
// Created by brucegoose on 4/15/23.
//

#ifndef SERVICE_HEADSET_STREAMER_HPP
#define SERVICE_HEADSET_STREAMER_HPP

#include <utility>

#include "utils/asio_context.hpp"
#include "infrastructure/tcp/tcp_client.hpp"
#include "infrastructure/websocket/websocket_client.hpp"
#include "infrastructure/decoder/decoder.hpp"
#include "infrastructure/graphics/graphics.hpp"
#include "infrastructure/gpio/gpio.hpp"

#include "domain/headset_domain.hpp"

namespace service {
    struct HeadsetStreamerConfig:
        public AsioContextConfig,
        public infrastructure::TcpClientConfig,
        public infrastructure::DecoderConfig,
        public infrastructure::GraphicsConfig,
        public infrastructure::GpioConfig,
        public infrastructure::WebsocketClientConfig
    {
        HeadsetStreamerConfig(
                std::string tcp_server_host, int tcp_server_port,
                int tcp_client_timeout_on_read,
                bool tcp_client_use_fixed_port,
                std::string websocket_server_host, int websocket_server_port,
                std::pair<int, int> image_width_height,
                int tcp_read_buffers, int decoder_buffers_downstream,
                infrastructure::DecoderType decoder_type,
                infrastructure::GraphicsType graphics_type,
                infrastructure::GpioType gpio_type
        ):
            _tcp_server_host(std::move(tcp_server_host)),
            _tcp_server_port(tcp_server_port),
            _tcp_client_timeout_on_read(tcp_client_timeout_on_read),
            _tcp_client_use_fixed_port(tcp_client_use_fixed_port),
            _tcp_read_buffers(tcp_read_buffers),
            _websocket_server_host(std::move(websocket_server_host)),
            _websocket_server_port(websocket_server_port),
            _decoder_type(decoder_type),
            _decoder_buffers_downstream(decoder_buffers_downstream),
            _graphics_type(graphics_type),
            _image_width_height(std::move(image_width_height)),
            _gpio_type(gpio_type)
        {}
        [[nodiscard]] int get_asio_pool_size() const override {
            return 2;
        }
        [[nodiscard]] std::string get_tcp_server_host() const override {
            return _tcp_server_host;
        }
        [[nodiscard]] int get_tcp_server_port() const override {
            return _tcp_server_port;
        }
        [[nodiscard]] ConnectionType get_tcp_client_connection_type() const override {
            return ConnectionType::HEADSET_CONNECTION;
        }
        [[nodiscard]] bool get_tcp_client_used_fixed_port() const override {
            return _tcp_client_use_fixed_port;
        }
        [[nodiscard]] std::string get_websocket_server_host() const override {
            return _websocket_server_host;
        };
        [[nodiscard]] int get_websocket_server_port() const override {
            return _websocket_server_port;
        };
        [[nodiscard]] int get_websocket_client_connection_timeout() const override {
            return 20;
        };
        [[nodiscard]] int get_websocket_client_op_timeout() const override {
            return 10;
        };
        [[nodiscard]] ConnectionType get_websocket_client_connection_type() const override {
            return ConnectionType::HEADSET_CONNECTION;
        };
        [[nodiscard]] bool get_websocket_client_used_fixed_port() const override {
            return _tcp_client_use_fixed_port;
        };
        [[nodiscard]] std::pair<int, int> get_image_width_height() const override {
            return _image_width_height;
        };
        [[nodiscard]] infrastructure::GraphicsType get_graphics_type() const override {
            return _graphics_type;
        };
        [[nodiscard]] infrastructure::DecoderType get_decoder_type() const override {
            return _decoder_type;
        };
        [[nodiscard]] unsigned int get_decoder_downstream_buffer_count() const override {
            return _decoder_buffers_downstream;
        };
        [[nodiscard]] std::pair<int, int> get_decoder_width_height() const override {
            return _image_width_height;
        };
        [[nodiscard]] int get_tcp_client_timeout_on_read() const override {
            return _tcp_client_timeout_on_read;
        };
        [[nodiscard]] int get_tcp_client_read_buffer_count() const override {
            return _tcp_read_buffers;
        };
        [[nodiscard]] int get_tcp_client_read_buffer_size() const override {
            return _image_width_height.first * _image_width_height.second * 3 / 2;
        };
        [[nodiscard]] infrastructure::GpioType get_gpio_type() const override {
            return _gpio_type;
        };
        [[nodiscard]] int get_button_pin() const override {
            return 17;
        };
        [[nodiscard]] int get_button_debounce_ms() const override {
            return 20;
        };
        [[nodiscard]] int get_button_hold_ms() const override {
            return 2500;
        };
        [[nodiscard]] int get_button_polling_ms() const override {
            return 15;
        }
    private:
        const std::string _tcp_server_host;
        const int _tcp_server_port;
        const int _tcp_client_timeout_on_read;
        const bool _tcp_client_use_fixed_port;
        const int _tcp_read_buffers;
        const std::string _websocket_server_host;
        const int _websocket_server_port;
        const infrastructure::DecoderType _decoder_type;
        const int _decoder_buffers_downstream;
        const std::pair<int, int> _image_width_height;
        const infrastructure::GraphicsType _graphics_type;
        const infrastructure::GpioType _gpio_type;
    };

    class HeadsetStreamer:
        public std::enable_shared_from_this<HeadsetStreamer>,
        public infrastructure::TcpClientManager,
        public infrastructure::WebsocketClientManager
    {
    public:
        static std::shared_ptr<HeadsetStreamer> Create(const HeadsetStreamerConfig &config);
        HeadsetStreamer(): _is_started(false) {}
        void Start() {
            if (_is_started) {
                return;
            }
            _state.PostState(domain::HeadsetStates::CONNECTING);
            _graphics->Start();
            _decoder->Start();
            _asio_context->Start();
            _tcp_client->Start();
            _websocket_client->Start();
            _gpio->Start();
            _is_started = true;
        }
        void Stop() {
            if (!_is_started) {
                return;
            }
            _state.PostState(domain::HeadsetStates::CLOSING);
            _gpio->Stop();
            _websocket_client->Stop();
            _tcp_client->Stop();
            _asio_context->Stop();
            _graphics->Stop();
            _decoder->Stop();
            _is_started = false;
        }
        void Unset() {
            _tcp_client.reset();
            _websocket_client.reset();
            _asio_context.reset();
            _graphics.reset();
            _decoder.reset();
            _gpio.reset();
        }
        // camera isn't an option so no need to initialize
        void CreateCameraClientConnection() override {};
        void DestroyCameraClientConnection() override {};

        void CreateHeadsetClientConnection() override;
        void PostHeadsetClientBuffer(std::shared_ptr<SizedBuffer> &&buffer) override;
        void DestroyHeadsetClientConnection() override;

        void CreateWebsocketClientConnection() override;
        [[nodiscard]] bool PostWebsocketServerMessage(nlohmann::json &&message) override;
        void DestroyWebsocketClientConnection() override;
    private:
        void initialize(const HeadsetStreamerConfig &config);
        void doStateChange(const domain::HeadsetStates state);
        void handleStateChange(const domain::HeadsetStates state);
        void handleStateChangeConnecting();
        std::atomic_bool _is_started = false;
        std::shared_ptr<AsioContext> _asio_context = nullptr;
        std::shared_ptr<infrastructure::TcpClient> _tcp_client = nullptr;
        infrastructure::WebsocketClientPtr _websocket_client = nullptr;
        std::shared_ptr<infrastructure::Graphics> _graphics = nullptr;
        std::shared_ptr<infrastructure::Decoder> _decoder = nullptr;
        std::shared_ptr<infrastructure::Gpio> _gpio = nullptr;
        domain::HeadsetState _state;
    };
}

#endif //SERVICE_HEADSET_STREAMER_HPP

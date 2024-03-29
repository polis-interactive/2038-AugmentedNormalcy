//
// Created by brucegoose on 4/15/23.
//

#ifndef SERVICE_DISPLAY_STREAMER_HPP
#define SERVICE_DISPLAY_STREAMER_HPP

#include <utility>

#include "utils/asio_context.hpp"
#include "infrastructure/tcp/tcp_client.hpp"
#include "infrastructure/websocket/websocket_client.hpp"
#include "infrastructure/decoder/decoder.hpp"
#include "infrastructure/graphics/graphics.hpp"

#include "domain/headset_domain.hpp"

namespace service {
    struct DisplayStreamerConfig:
            public AsioContextConfig,
            public infrastructure::TcpClientConfig,
            public infrastructure::DecoderConfig,
            public infrastructure::GraphicsConfig,
            public infrastructure::WebsocketClientConfig
    {
        DisplayStreamerConfig(
                std::string tcp_server_host, int tcp_server_port,
                int tcp_client_timeout_on_read,
                bool tcp_client_use_fixed_port,
                std::string websocket_server_host, int websocket_server_port,
                std::pair<int, int> image_width_height,
                int tcp_read_buffers, int decoder_buffers_downstream,
                infrastructure::DecoderType decoder_type,
                infrastructure::GraphicsType graphics_type,
                int switch_automatic_timeout
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
                _switch_automatic_timeout(switch_automatic_timeout)
        {}
        [[nodiscard]] int get_asio_pool_size() const override {
            return 3;
        }
        [[nodiscard]] std::string get_tcp_server_host() const override {
            return _tcp_server_host;
        }
        [[nodiscard]] int get_tcp_server_port() const override {
            return _tcp_server_port;
        }
        [[nodiscard]] ConnectionType get_tcp_client_connection_type() const override {
            return ConnectionType::DISPLAY_CONNECTION;
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
            return ConnectionType::DISPLAY_CONNECTION;
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
        [[nodiscard]] int get_server_camera_switching_automatic_timeout() const {
            return _switch_automatic_timeout;
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
        const int _switch_automatic_timeout;
    };

    class DisplayStreamer:
            public std::enable_shared_from_this<DisplayStreamer>,
            public infrastructure::TcpClientManager,
            public infrastructure::WebsocketClientManager
    {
    public:
        static std::shared_ptr<DisplayStreamer> Create(const DisplayStreamerConfig &config);
        DisplayStreamer(DisplayStreamerConfig config);

        void Start();
        void Stop();
        void Unset();

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
        void initialize();
        void runAutomaticSwitching();

        const DisplayStreamerConfig _conf;

        std::atomic_bool _is_started = false;
        std::shared_ptr<AsioContext> _asio_context = nullptr;
        std::shared_ptr<infrastructure::TcpClient> _tcp_client = nullptr;
        infrastructure::WebsocketClientPtr _websocket_client = nullptr;
        std::shared_ptr<infrastructure::Graphics> _graphics = nullptr;
        std::shared_ptr<infrastructure::Decoder> _decoder = nullptr;

        std::unique_ptr<std::thread> _work_thread;
        std::atomic<bool> _work_stop = { true };
    };
}

#endif //SERVICE_DISPLAY_STREAMER_HPP

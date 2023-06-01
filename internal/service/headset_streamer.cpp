//
// Created by brucegoose on 4/15/23.
//

#include "headset_streamer.hpp"

namespace service {

    std::shared_ptr<HeadsetStreamer> HeadsetStreamer::Create(const service::HeadsetStreamerConfig &config) {
        auto headset_streamer = std::make_shared<HeadsetStreamer>();
        headset_streamer->initialize(config);
        return headset_streamer;
    }

    void HeadsetStreamer::initialize(const service::HeadsetStreamerConfig &config) {
        _asio_context = AsioContext::Create(config);
        _tcp_client = infrastructure::TcpClient::Create(config, _asio_context->GetContext(), shared_from_this());
        _websocket_client = infrastructure::WebsocketClient::Create(
            config, _asio_context->GetContext(), shared_from_this()
        );
        _graphics = infrastructure::Graphics::Create(config);
        auto self(shared_from_this());
        _decoder = infrastructure::Decoder::Create(
            config,
            [this, self](std::shared_ptr<DecoderBuffer> &&buffer) {
                _graphics->PostImage(std::move(buffer));
            }
        );
        _gpio = infrastructure::Gpio::Create(
            config,
            [this, self]() {

            }
        );
        _bms = infrastructure::Bms::Create(
            config, _asio_context->GetContext(), [this, self](const BmsMessage message) {

            }
        );
    }

    void HeadsetStreamer::PostHeadsetClientBuffer(std::shared_ptr<SizedBuffer> &&buffer) {
        _decoder->PostJpegBuffer(std::move(buffer));
    }

    bool HeadsetStreamer::PostWebsocketServerMessage(nlohmann::json &&message) {
        throw std::runtime_error("HeadsetStreamer::PostWebsocketServerMessage not implemented");
    }
}
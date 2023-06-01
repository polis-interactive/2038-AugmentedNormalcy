//
// Created by brucegoose on 4/15/23.
//

#include "domain/message.hpp"

#include "display_streamer.hpp"


namespace service {

    std::shared_ptr<DisplayStreamer> DisplayStreamer::Create(const DisplayStreamerConfig &config) {
        auto headset_streamer = std::make_shared<DisplayStreamer>();
        headset_streamer->initialize(config);
        return headset_streamer;
    }

    void DisplayStreamer::initialize(const DisplayStreamerConfig &config) {
        _asio_context = AsioContext::Create(config);
        auto self(shared_from_this());
        _tcp_client = infrastructure::TcpClient::Create(config, _asio_context->GetContext(), std::move(self));
        _websocket_client = infrastructure::WebsocketClient::Create(
            config, _asio_context->GetContext(), shared_from_this()
        );
        _graphics = infrastructure::Graphics::Create(config);
        self = shared_from_this();
        _decoder = infrastructure::Decoder::Create(
                config,
                [this, self](std::shared_ptr<DecoderBuffer> &&buffer) {
                    _graphics->PostImage(std::move(buffer));
                }
        );
    }

    void DisplayStreamer::CreateHeadsetClientConnection() {
        // eventually, we probably want to track this
    }

    void DisplayStreamer::PostHeadsetClientBuffer(std::shared_ptr<SizedBuffer> &&buffer) {
        _decoder->PostJpegBuffer(std::move(buffer));
    }

    void DisplayStreamer::DestroyHeadsetClientConnection() {
        // eventually, we probably want to track this
    }

    void DisplayStreamer::CreateWebsocketClientConnection() {
        // eventually, we probably want to track this
    }

    bool DisplayStreamer::PostWebsocketServerMessage(nlohmann::json &&message) {
        throw std::runtime_error("HeadsetStreamer::PostWebsocketServerMessage not implemented");
    }

    void DisplayStreamer::DestroyWebsocketClientConnection() {
        // eventually, we probably want to track this
    }

}
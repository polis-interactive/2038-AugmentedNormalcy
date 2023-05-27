//
// Created by brucegoose on 4/15/23.
//

#include "headset_streamer.hpp"

// probably need to check if we are on the pi here...

static void turnOnScreen() {
    std::string command = "uhubctl -l 1-1 -a 1";
    std::system(command.c_str());
}

static void turnOffScreen() {
    std::string command = "uhubctl -l 1-1 -a 0";
    std::system(command.c_str());
}

namespace service {

    std::shared_ptr<HeadsetStreamer> HeadsetStreamer::Create(const service::HeadsetStreamerConfig &config) {
        auto headset_streamer = std::make_shared<HeadsetStreamer>();
        headset_streamer->initialize(config);
        return headset_streamer;
    }

    void HeadsetStreamer::initialize(const service::HeadsetStreamerConfig &config) {
        _asio_context = AsioContext::Create(config);
        auto self(shared_from_this());
        _tcp_client = infrastructure::TcpClient::Create(config, _asio_context->GetContext(), std::move(self));
        _graphics = infrastructure::Graphics::Create(config);
        self = shared_from_this();
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

    void HeadsetStreamer::CreateHeadsetClientConnection() {
        const auto [state_change, state] = _state.PostTcpConnection(true);
        if (state_change) {
            handleStateChange(state);
        }
    }

    void HeadsetStreamer::handleStateChange(const domain::HeadsetStates state) {
        switch (state) {
            case domain::HeadsetStates::CONNECTING:
                handleStateChangeConnecting();
                break;
            case domain::HeadsetStates::READY:
                // nothing to do as graphics loop auto pulls state
                break;
            case domain::HeadsetStates::RUNNING:
                // nothing to do as graphics loop auto pulls state
                break;
            case domain::HeadsetStates::PLUGGED_IN:
                handleStateChangePluggedIn();
                break;
            case domain::HeadsetStates::DYING:
                handleStateChangeDying();
                break;
        }
    }

    void HeadsetStreamer::handleStateChangeConnecting() {
        // here, we assume we were either plugged in; can universally apply though as we don't care how we got here
        turnOnScreen();
        _tcp_client->Start();
        _decoder->Start();
    }

    void HeadsetStreamer::handleStateChangePluggedIn() {
        turnOffScreen();
        _tcp_client->Stop();
        _decoder->Stop();
    }

    void HeadsetStreamer::handleStateChangeDying() {
        _tcp_client->Stop();
        _decoder->Stop();
    }


}
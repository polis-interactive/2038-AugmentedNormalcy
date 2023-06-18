//
// Created by brucegoose on 4/15/23.
//

#include "domain/message.hpp"

#include "headset_streamer.hpp"

// not using, but maybe we can dim the screen instead?

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
            [this, self](const domain::ButtonAction action) {
                // assume action is never NULL_ACTION
                const auto state = _state.GetState();
                if (state == domain::HeadsetStates::READY) {
                    // we really don't care about the action here
                    doStateChange(domain::HeadsetStates::RUNNING);
                } else if (state == domain::HeadsetStates::RUNNING) {
                    if (action == domain::ButtonAction::SINGLE_HOLD) {
                        doStateChange(domain::HeadsetStates::READY);
                    } else {
                        _websocket_client->PostWebsocketClientMessage(
                            domain::RotateCameraMessage().GetMessage()
                        );
                    }
                }
            }
        );
    }

    void HeadsetStreamer::CreateHeadsetClientConnection() {
        const auto [state_change, state] = _state.PostTcpConnection(true);
        if (state_change) {
            handleStateChange(state);
        }
    }

    void HeadsetStreamer::PostHeadsetClientBuffer(std::shared_ptr<SizedBuffer> &&buffer) {
        _decoder->PostJpegBuffer(std::move(buffer));
    }

    void HeadsetStreamer::DestroyHeadsetClientConnection() {
        const auto [state_change, state] = _state.PostTcpConnection(false);
        if (state_change) {
            handleStateChange(state);
        }
    }

    void HeadsetStreamer::CreateWebsocketClientConnection() {
        const auto [state_change, state] = _state.PostWebsocketConnection(true);
        if (state_change) {
            handleStateChange(state);
        }
    }

    bool HeadsetStreamer::PostWebsocketServerMessage(nlohmann::json &&message) {
        throw std::runtime_error("HeadsetStreamer::PostWebsocketServerMessage not implemented");
    }

    void HeadsetStreamer::DestroyWebsocketClientConnection() {
        const auto [state_change, state] = _state.PostWebsocketConnection(false);
        if (state_change) {
            handleStateChange(state);
        }
    }

    void HeadsetStreamer::doStateChange(const domain::HeadsetStates state) {
        _state.PostState(state);
        handleStateChange(state);
    }

    void HeadsetStreamer::handleStateChange(const domain::HeadsetStates state) {
        _graphics->PostGraphicsHeadsetState(state);
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
            case domain::HeadsetStates::CLOSING:
                // nothing to do as we are getting the fk out of here
                break;
        }
    }

    void HeadsetStreamer::handleStateChangeConnecting() {
        // here, we assume we were either plugged in; can universally apply though as we don't care how we got here
        // turnOnScreen();
        _websocket_client->Start();
        _tcp_client->Start();
        _decoder->Start();
    }
}
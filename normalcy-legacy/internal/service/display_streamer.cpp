//
// Created by brucegoose on 4/15/23.
//

#include "utils/clock.hpp"
#include "domain/headset_domain.hpp"

#include "display_streamer.hpp"


namespace service {

    std::shared_ptr<DisplayStreamer> DisplayStreamer::Create(const DisplayStreamerConfig &config) {
        auto headset_streamer = std::make_shared<DisplayStreamer>(config);
        headset_streamer->initialize();
        return headset_streamer;
    }
    DisplayStreamer::DisplayStreamer(DisplayStreamerConfig config):
        _is_started(false), _conf(std::move(config))
    {}

    void DisplayStreamer::initialize() {
        _asio_context = AsioContext::Create(_conf);
        auto self(shared_from_this());
        _tcp_client = infrastructure::TcpClient::Create(_conf, _asio_context->GetContext(), std::move(self));
        _websocket_client = infrastructure::WebsocketClient::Create(
            _conf, _asio_context->GetContext(), shared_from_this()
        );
        _graphics = infrastructure::Graphics::Create(_conf);
        self = shared_from_this();
        _decoder = infrastructure::Decoder::Create(
                _conf,
                [this, self](std::shared_ptr<DecoderBuffer> &&buffer) {
                    _graphics->PostImage(std::move(buffer));
                }
        );
    }

    void DisplayStreamer::Start() {
        if (_is_started) {
            return;
        }
        /* startup worker thread */
        _work_stop = false;
        _work_thread = std::make_unique<std::thread>(
            [this, self = shared_from_this()]() mutable {
                runAutomaticSwitching();
            }
        );

        _graphics->Start();
        _decoder->Start();
        _asio_context->Start();
        _tcp_client->Start();
        _websocket_client->Start();
        _is_started = true;
    }

    void DisplayStreamer::Stop() {
        if (!_is_started) {
            return;
        }
        /* teardown thread */
        if (_work_thread) {
            if (_work_thread->joinable()) {
                _work_stop = true;
                _work_thread->join();
            }
            _work_thread.reset();
        } else {
            _work_stop = true;
        }

        _websocket_client->Stop();
        _tcp_client->Stop();
        _asio_context->Stop();
        _graphics->Stop();
        _decoder->Stop();
        _is_started = false;
    }

    void DisplayStreamer::Unset() {
        _tcp_client.reset();
        _websocket_client.reset();
        _asio_context.reset();
        _graphics.reset();
        _decoder.reset();
    }

    void DisplayStreamer::runAutomaticSwitching() {
        const auto automatic_timeout = _conf.get_server_camera_switching_automatic_timeout();
        auto last_timer = Clock::now();
        while(!_work_stop) {
            const auto now = Clock::now();
            const auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_timer);
            if (duration.count() < automatic_timeout) {
                std::this_thread::sleep_for(1s);
                continue;
            }
            _websocket_client->PostWebsocketClientMessage(domain::HeadsetRotateCameraMessage().GetMessage());
            last_timer = now;
        }
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
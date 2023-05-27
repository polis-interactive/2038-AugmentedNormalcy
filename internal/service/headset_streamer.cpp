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

    void HeadsetStreamer::PostHeadsetClientBuffer(std::shared_ptr<SizedBuffer> &&buffer) {
        _decoder->PostJpegBuffer(std::move(buffer));
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
                std::cout << "Button pushed!" << std::endl;
            }
        );
        _bms = infrastructure::Bms::Create(
            config, _asio_context->GetContext(), [this, self](const BmsMessage message) {
                std::cout << "BMS Reports: " << std::endl;
                std::cout << "Battery level (out of 100) " << message.battery_level << std::endl;
                std::cout << "Battery is plugged in? " << message.bms_is_plugged_in << std::endl;
                std::cout << "BMS wants shutdown? " << message.bms_wants_shutdown << std::endl;
            }
        );
    }
}
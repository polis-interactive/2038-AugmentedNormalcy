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
        _tcp_context = infrastructure::TcpContext::Create(config);
        auto self(shared_from_this());
        _tcp_client = infrastructure::TcpClient::Create(config, _tcp_context->GetContext(), std::move(self));
        _graphics = infrastructure::Graphics::Create(config);
        self = shared_from_this();
        _decoder = infrastructure::Decoder::Create(
            config,
            [this, s = std::move(self)](std::shared_ptr<DecoderBuffer> &&buffer) {
                _graphics->PostImage(std::move(buffer));
            }
        );
    }
}
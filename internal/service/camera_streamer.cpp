//
// Created by brucegoose on 3/22/23.
//

#include "camera_streamer.hpp"

namespace service {

    std::shared_ptr<CameraStreamer> CameraStreamer::Create(const service::CameraStreamerConfig &config) {
        auto camera_streamer = std::make_shared<CameraStreamer>();
        camera_streamer->initialize(config);
        return camera_streamer;
    }

    void CameraStreamer::initialize(const service::CameraStreamerConfig &config) {
        _tcp_context = infrastructure::TcpContext::Create(config);
        auto self(shared_from_this());
        _tcp_client = infrastructure::TcpClient::Create(config, _tcp_context->GetContext(), self);
        _camera = infrastructure::Camera::Create(config, [this, self](std::shared_ptr<SizedBuffer> &&post_buffer) {
            _tcp_client->Post(std::move(post_buffer));
        });
    }
}
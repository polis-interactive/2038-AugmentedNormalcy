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
        _asio_context = AsioContext::Create(config);
        auto self(shared_from_this());
        _tcp_client = infrastructure::TcpClient::Create(config, _asio_context->GetContext(), self);
        _encoder = infrastructure::Encoder::Create(
            config,
            [this, self](std::shared_ptr<SizedBuffer> &&buffer) {
                _tcp_client->Post(std::move(buffer));
            }
        );
        _camera = infrastructure::Camera::Create(
            config,
            [this, self](std::shared_ptr<CameraBuffer> &&camera_buffer) {
                _encoder->PostCameraBuffer(std::move(camera_buffer));
            }
        );
    }
}
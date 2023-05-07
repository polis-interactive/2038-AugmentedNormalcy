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
        _encoder = infrastructure::SwEncoder::Create(
            config,
            [this, self](std::shared_ptr<SizedBuffer> &&buffer) {
                std::cout << "Post encoder" << std::endl;
                _tcp_client->Post(std::move(buffer));
            }
        );
        _camera = infrastructure::LibcameraCamera::Create(
            config,
            [this, self](std::shared_ptr<CameraBuffer> &&camera_buffer) {
                std::cout << "Post camera" << std::endl;
                _encoder->PostCameraBuffer(std::move(camera_buffer));
            }
        );
    }
}
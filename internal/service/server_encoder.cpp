//
// Created by brucegoose on 4/8/23.
//

#include "server_encoder.hpp"

namespace service {
    std::shared_ptr<ServerEncoder> ServerEncoder::Create(const service::ServerEncoderConfig &config) {
        auto camera_streamer = std::make_shared<ServerEncoder>(config);
        camera_streamer->initialize();
        return camera_streamer;
    }

    void ServerEncoder::initialize() {
        _tcp_context = infrastructure::TcpContext::Create(_conf);
        auto self(shared_from_this());
        _tcp_server = infrastructure::TcpServer::Create(_conf, _tcp_context->GetContext(), self);
    }

    infrastructure::TcpConnectionType ServerEncoder::GetConnectionType(tcp::endpoint endpoint) {
        std::unique_lock<std::mutex> camera_lock(_camera_mutex, std::defer_lock);
        std::unique_lock<std::mutex> headset_lock(_headset_mutex, std::defer_lock);
        std::lock(_camera_mutex, _headset_mutex);
        /*
         * for now, we just assume camera connects first, and then headset
         */
        if (!_camera_session) {
            return infrastructure::TcpConnectionType::CAMERA_CONNECTION;
        } else if (!_headset_session) {
            return infrastructure::TcpConnectionType::HEADSET_CONNECTION;
        }
        return infrastructure::TcpConnectionType::UNKNOWN_CONNECTION;
    }

    infrastructure::CameraConnectionPayload ServerEncoder::CreateCameraServerConnection(
            std::shared_ptr<infrastructure::TcpSession> camera_session
    ) {
        {
            std::unique_lock<std::mutex> lock(_camera_mutex);
            if (_camera_session) {
                _camera_session->TryClose();
                _camera_session.reset();
            }
            _camera_session = camera_session;
        }
        auto self(shared_from_this());
        auto enc = infrastructure::Encoder::Create(
            _conf, [this, s = std::move(self)] (std::shared_ptr<SizedBuffer> &&buffer) {
                std::shared_ptr<infrastructure::WritableTcpSession> headset_session;
                {
                    std::unique_lock<std::mutex> lock(_headset_mutex);
                    headset_session = _headset_session;
                }
                if (headset_session) {
                    headset_session->Write(std::move(buffer));
                }
            }
        );
        return { ++_last_session_number, enc };
    }

    void ServerEncoder::DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> camera_session) {
        std::unique_lock<std::mutex> lock(_camera_mutex);
        if (_camera_session && _camera_session->GetSessionId() == camera_session->GetSessionId()) {
            _camera_session->TryClose();
            _camera_session.reset();
        }
    }

    unsigned long ServerEncoder::CreateHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> headset_session
    ) {
        {
            std::unique_lock<std::mutex> lock(_headset_mutex);
            if (_headset_session) {
                _headset_session->TryClose();
                _headset_session.reset();
            }
            _headset_session = headset_session;
        }
        return ++_last_session_number;
    }

    void ServerEncoder::DestroyHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> headset_session
    ) {
        std::unique_lock<std::mutex> lock(_headset_mutex);
        if (_headset_session && _headset_session->GetSessionId() == headset_session->GetSessionId()) {
            _headset_session->TryClose();
            _headset_session.reset();
        }
    }
}
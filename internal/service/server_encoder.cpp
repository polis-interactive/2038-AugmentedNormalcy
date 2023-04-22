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
        auto addr = endpoint.address().to_string();
        if (addr == "192.168.1.110") {
            return infrastructure::TcpConnectionType::CAMERA_CONNECTION;
        } else if (addr == "192.168.1.100") {
            return infrastructure::TcpConnectionType::HEADSET_CONNECTION;
        }
        return infrastructure::TcpConnectionType::UNKNOWN_CONNECTION;
    }

    infrastructure::CameraConnectionPayload ServerEncoder::CreateCameraServerConnection(
            std::shared_ptr<infrastructure::TcpSession> camera_session
    ) {
        {
            std::cout << "Is someone hanging onto the mutex?" << std::endl;
            std::unique_lock<std::mutex> lock(_camera_mutex);
            std::cout << "no..." << std::endl;
            if (_camera_session != nullptr) {
                std::cout << "cpp about to hang" << std::endl;
                _camera_session->TryClose(false);
                std::cout << "cpp not hung" << std::endl;
                _camera_session.reset();
            }
            _camera_session = camera_session;
        }
        std::cout << "make it here" << std::endl;
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
        std::cout << "make it out" << std::endl;
        return { ++_last_session_number, enc };
    }

    void ServerEncoder::DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> camera_session) {
        std::unique_lock<std::mutex> lock(_camera_mutex);
        if (_camera_session && _camera_session->GetSessionId() == camera_session->GetSessionId()) {
            _camera_session.reset();
        }
    }

    unsigned long ServerEncoder::CreateHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> headset_session
    ) {
        {
            std::unique_lock<std::mutex> lock(_headset_mutex);
            if (_headset_session) {
                _headset_session->TryClose(false);
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
            _headset_session.reset();
        }
    }
}
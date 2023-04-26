//
// Created by brucegoose on 4/8/23.
//

#include "server_encoder.hpp"

static tcp_addr ip_bound(const std::string& ip_string) {
    return tcp_addr::from_string(ip_string);
}

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

    void ServerEncoder::Start() {
        if (_is_started) {
            return;
        }
        /* startup worker thread */
        _work_stop = false;
        auto self(shared_from_this());
        _work_thread = std::make_unique<std::thread>([this, s = std::move(self)]() mutable { run(); });
        /* startup server */
        _tcp_context->Start();
        _tcp_server->Start();
        _is_started = true;
    }

    void ServerEncoder::run() {
        while(!_work_stop) {
            // do some cin here
        }
    }

    infrastructure::TcpConnectionType ServerEncoder::GetConnectionType(tcp::endpoint endpoint) {
        static const tcp_addr min_headset_address = ip_bound("192.168.1.100");
        static const tcp_addr min_camera_address = ip_bound("192.168.1.200");
        const tcp_addr addr_endpoint = endpoint.address().to_v4();
        if (addr_endpoint >= min_camera_address) {
            return infrastructure::TcpConnectionType::CAMERA_CONNECTION;
        } else if (addr_endpoint >= min_headset_address) {
            return infrastructure::TcpConnectionType::HEADSET_CONNECTION;
        }
        return infrastructure::TcpConnectionType::UNKNOWN_CONNECTION;
    }

    infrastructure::CameraConnectionPayload ServerEncoder::CreateCameraServerConnection(
            std::shared_ptr<infrastructure::TcpSession> camera_session
    ) {
        bool is_only_camera = false;
        const auto endpoint = camera_session->GetEndpoint();
        {
            std::unique_lock<std::mutex> lock(_camera_mutex);
            is_only_camera = _camera_sessions.empty();
            auto find_session = _camera_sessions.find(camera_session->GetEndpoint());
            if (find_session != _camera_sessions.end()) {
                find_session->second->TryClose(true);
                find_session->second.reset();
            }
            _camera_sessions[endpoint] = camera_session;
        }

        if (is_only_camera) {
            std::unique_lock<std::mutex> lock(_headset_mutex);
            _connection_manager.AddManyMappings(endpoint, _headset_sessions);
        }

        auto self(shared_from_this());
        auto enc = infrastructure::Encoder::Create(
            _conf,
            [this, s = std::move(self), endpoint] (std::shared_ptr<SizedBuffer> &&buffer) {
                _connection_manager.PostMessage(endpoint, std::move(buffer));
            }
        );
        return { ++_last_session_number, enc };
    }

    void ServerEncoder::DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> camera_session) {

        const auto endpoint = camera_session->GetEndpoint();

        std::unique_lock<std::mutex> lock(_camera_mutex);
        auto find_session = _camera_sessions.find(endpoint);
        if (
                find_session != _camera_sessions.end() &&
                find_session->second->GetSessionId() == camera_session->GetSessionId()
        ) {
            _connection_manager.RemoveCamera(endpoint);
            find_session->second->TryClose(false);
            find_session->second.reset();
            _camera_sessions.erase(find_session);
        } else if (find_session == _camera_sessions.end()) {
            _connection_manager.RemoveCamera(endpoint);
        }

    }

    unsigned long ServerEncoder::CreateHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> headset_session
    ) {

        const auto endpoint = headset_session->GetEndpoint();

        {
            std::unique_lock<std::mutex> lock(_headset_mutex);
            auto find_session = _headset_sessions.find(headset_session->GetEndpoint());
            if (find_session != _headset_sessions.end()) {
                find_session->second->TryClose(true);
                find_session->second.reset();
            }
            _headset_sessions[endpoint] = headset_session;
        }

        bool did_change_mapping = _connection_manager.TryReplaceSession(
                headset_session->GetEndpoint(), headset_session
        );

        if (!did_change_mapping) {
            std::unique_lock<std::mutex> lock(_camera_mutex);
            if (_camera_sessions.begin() != _camera_sessions.end()) {
                _connection_manager.AddMapping(_camera_sessions.begin()->first, endpoint, headset_session);
            }
        }

        return ++_last_session_number;
    }

    void ServerEncoder::DestroyHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> headset_session
    ) {

        const auto endpoint = headset_session->GetEndpoint();

        std::unique_lock<std::mutex> lock(_headset_mutex);
        auto find_session = _headset_sessions.find(endpoint);
        if (
                find_session != _headset_sessions.end() &&
                find_session->second->GetSessionId() == headset_session->GetSessionId()
        ) {
            _connection_manager.RemoveHeadset(headset_session->GetEndpoint());
            find_session->second->TryClose(false);
            find_session->second.reset();
            _headset_sessions.erase(find_session);
        } else if (find_session == _headset_sessions.end()) {
            _connection_manager.RemoveHeadset(endpoint);
        }

    }

    void ServerEncoder::Stop() {
        if (!_is_started) {
            return;
        }
        /* cleanup connections */
        _connection_manager.Clear();

        /* cleanup sessions */
        {
            std::unique_lock<std::mutex> lock(_camera_mutex);
            for (auto &[endpoint, camera] : _camera_sessions) {
                camera->TryClose(false);
                camera.reset();
            }
            _camera_sessions.clear();
        }
        {
            std::unique_lock<std::mutex> lock(_headset_mutex);
            for (auto &[endpoint, headset] : _headset_sessions) {
                headset->TryClose(false);
                headset.reset();
            }
            _headset_sessions.clear();
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
        /* stop server */
        _tcp_server->Stop();
        _tcp_context->Stop();
        _is_started = false;
    }
}
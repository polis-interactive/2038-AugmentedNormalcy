//
// Created by brucegoose on 4/8/23.
//

#include "server_encoder.hpp"

static const tcp_addr ip_bound(const std::string& ip_string) {
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

    void ServerEncoder::run() {
        while(!_work_stop) {
            InputBuffer buffer;
            {
                std::unique_lock<std::mutex> lock(_work_mutex);
                _work_cv.wait(lock, [this]() {
                    return !_work_queue.empty() || _work_stop;
                });
                if (_work_stop) {
                    return;
                } else if (_work_queue.empty()) {
                    continue;
                }
                buffer = std::move(_work_queue.front());
                _work_queue.pop();
            }
            shipBuffer(std::move(buffer));
        }
    }

    void ServerEncoder::shipBuffer(InputBuffer &&input) {
        std::size_t camera_count;
        {
            std::unique_lock<std::mutex> lock(_camera_mutex);
            camera_count = _camera_sessions.size();
        }
        if (!_has_processed) {
            _last_endpoint = input.first;
            _last_camera_swap = SteadyClock::now();
            _has_processed = true;
        } else if (camera_count == 1) {
            // inelegant but miss me with that duration stuff
        } else if (input.first != _last_endpoint) {
            const auto now = SteadyClock::now();
            const auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(now - _last_camera_swap);
            if (elapsed_time.count() < 15) return;
            _last_endpoint = input.first;
            _last_camera_swap = now;
        }
        {
            std::unique_lock<std::mutex> lock(_headset_mutex);
            for (auto &[endpoint, headset]: _headset_sessions) {
                auto buffer_copy = input.second;
                headset->Write(std::move(buffer_copy));
            }
        }
        input.second.reset();
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
        {
            std::unique_lock<std::mutex> lock(_camera_mutex);
            auto find_session = _camera_sessions.find(camera_session->GetEndpoint());
            if (find_session != _camera_sessions.end()) {
                find_session->second->TryClose(true);
                find_session->second.reset();
            }
            _camera_sessions[camera_session->GetEndpoint()] = camera_session;
        }
        auto self(shared_from_this());
        auto enc = infrastructure::Encoder::Create(
            _conf,
            [this, s = std::move(self), endpoint = camera_session->GetEndpoint()] (std::shared_ptr<SizedBuffer> &&buffer) {
                if (_work_stop) {
                    return;
                }
                std::unique_lock<std::mutex> lock(_work_mutex);
                _work_queue.push({ endpoint, std::move(buffer) });
                _work_cv.notify_one();
            }
        );
        return { ++_last_session_number, enc };
    }


    void ServerEncoder::DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> camera_session) {
        std::unique_lock<std::mutex> lock(_camera_mutex);
        auto find_session = _camera_sessions.find(camera_session->GetEndpoint());
        if (
                find_session != _camera_sessions.end() &&
                find_session->second->GetSessionId() == camera_session->GetSessionId()
        ) {
            find_session->second->TryClose(false);
            find_session->second.reset();
            _camera_sessions.erase(find_session);
        }
    }

    unsigned long ServerEncoder::CreateHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> headset_session
    ) {
        {
            std::unique_lock<std::mutex> lock(_headset_mutex);
            auto find_session = _headset_sessions.find(headset_session->GetEndpoint());
            if (find_session != _headset_sessions.end()) {
                find_session->second->TryClose(true);
                find_session->second.reset();
            }
            _headset_sessions[headset_session->GetEndpoint()] = headset_session;
        }
        return ++_last_session_number;
    }

    void ServerEncoder::DestroyHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> headset_session
    ) {
        std::unique_lock<std::mutex> lock(_headset_mutex);
        auto find_session = _headset_sessions.find(headset_session->GetEndpoint());
        if (
                find_session != _headset_sessions.end() &&
                find_session->second->GetSessionId() == headset_session->GetSessionId()
                ) {
            find_session->second->TryClose(false);
            find_session->second.reset();
            _headset_sessions.erase(find_session);
        }
    }
}
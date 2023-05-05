//
// Created by brucegoose on 4/8/23.
//

#include "server_streamer.hpp"

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
        const tcp_addr camera_1_addr = ip_bound("192.168.1.200");
        const tcp_addr camera_2_addr = ip_bound("192.168.1.201");
        while(!_work_stop) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(STDIN_FILENO, &readfds);

            struct timeval timeout;
            timeout.tv_sec = 1;  // Wait up to 1 second
            timeout.tv_usec = 0;

            int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);

            if (ret > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
                int value;
                std::cin >> value;

                if (std::cin.fail()) {
                    std::cin.clear(); // Clear the error flags
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Ignore the rest of the line
                    std::cout << "Invalid input. Please enter 200 or 201." << std::endl;
                } else if (value == 200 || value == 201) {
                    auto &use_addr = value == 200 ? camera_1_addr : camera_2_addr;
                    std::unique_lock<std::mutex> lock(_camera_mutex);
                    const auto &find_session = _camera_sessions.find(use_addr);
                    if (find_session != _camera_sessions.end()) {
                        std::cout << "Switching to camera: " <<  value << std::endl;
                        _connection_manager.ChangeAllMappings(find_session->first);
                    } else {
                        std::cout << "Camera " << value << " is unavailable" << std::endl;
                    }
                } else {
                    std::cout << "Invalid input. Please enter 200 or 201." << std::endl;
                }
            } else if (ret == 0) {
                // Timeout, no input available
                continue;
            } else {
                // Error occurred
                break;
            }
        }
    }

    infrastructure::TcpConnectionType ServerEncoder::GetConnectionType(tcp_addr addr) {
        static const tcp_addr min_headset_address = ip_bound("192.168.1.100");
        static const tcp_addr min_camera_address = ip_bound("192.168.1.200");
        if (addr >= min_camera_address) {
            return infrastructure::TcpConnectionType::CAMERA_CONNECTION;
        } else if (addr >= min_headset_address) {
            return infrastructure::TcpConnectionType::HEADSET_CONNECTION;
        }
        return infrastructure::TcpConnectionType::UNKNOWN_CONNECTION;
    }

    unsigned long ServerEncoder::CreateCameraServerConnection(
            std::shared_ptr<infrastructure::TcpSession> camera_session
    ) {
        bool is_only_camera = false;
        const auto addr = camera_session->GetAddr();
        {
            std::unique_lock<std::mutex> lock(_camera_mutex);
            is_only_camera = _camera_sessions.empty();
            auto find_session = _camera_sessions.find(camera_session->GetAddr());
            if (find_session != _camera_sessions.end()) {
                find_session->second->TryClose(true);
                find_session->second.reset();
            }
            _camera_sessions[addr] = camera_session;
        }

        if (is_only_camera) {
            std::unique_lock<std::mutex> lock(_headset_mutex);
            _connection_manager.AddManyMappings(addr, _headset_sessions);
        }
        return ++_last_session_number;
    }

    void ServerEncoder::PostCameraServerBuffer(const tcp_addr &addr, std::shared_ptr<ResizableBuffer> &&buffer) {
        _connection_manager.PostMessage(addr, std::move(buffer));
    }

    void ServerEncoder::DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> camera_session) {

        const auto addr = camera_session->GetAddr();

        std::unique_lock<std::mutex> lock(_camera_mutex);
        auto find_session = _camera_sessions.find(addr);
        if (
                find_session != _camera_sessions.end() &&
                find_session->second->GetSessionId() == camera_session->GetSessionId()
        ) {
            _connection_manager.RemoveCamera(addr);
            find_session->second->TryClose(false);
            find_session->second.reset();
            _camera_sessions.erase(find_session);
        } else if (find_session == _camera_sessions.end()) {
            _connection_manager.RemoveCamera(addr);
        }

    }

    unsigned long ServerEncoder::CreateHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> headset_session
    ) {

        const auto addr = headset_session->GetAddr();

        {
            std::unique_lock<std::mutex> lock(_headset_mutex);
            auto find_session = _headset_sessions.find(headset_session->GetAddr());
            if (find_session != _headset_sessions.end()) {
                find_session->second->TryClose(true);
                find_session->second.reset();
            }
            _headset_sessions[addr] = headset_session;
        }

        bool did_change_mapping = _connection_manager.TryReplaceSession(
                headset_session->GetAddr(), headset_session
        );

        if (!did_change_mapping) {
            std::unique_lock<std::mutex> lock(_camera_mutex);
            if (_camera_sessions.begin() != _camera_sessions.end()) {
                _connection_manager.AddMapping(_camera_sessions.begin()->first, addr, headset_session);
            }
        }

        return ++_last_session_number;
    }

    void ServerEncoder::DestroyHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> headset_session
    ) {

        const auto addr = headset_session->GetAddr();

        std::unique_lock<std::mutex> lock(_headset_mutex);
        auto find_session = _headset_sessions.find(addr);
        if (
                find_session != _headset_sessions.end() &&
                find_session->second->GetSessionId() == headset_session->GetSessionId()
        ) {
            _connection_manager.RemoveHeadset(headset_session->GetAddr());
            find_session->second->TryClose(false);
            find_session->second.reset();
            _headset_sessions.erase(find_session);
        } else if (find_session == _headset_sessions.end()) {
            _connection_manager.RemoveHeadset(addr);
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
            for (auto &[_, camera] : _camera_sessions) {
                camera->TryClose(false);
                camera.reset();
            }
            _camera_sessions.clear();
        }
        {
            std::unique_lock<std::mutex> lock(_headset_mutex);
            for (auto &[_, headset] : _headset_sessions) {
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
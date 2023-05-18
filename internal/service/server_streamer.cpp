//
// Created by brucegoose on 4/8/23.
//

#include "server_streamer.hpp"

#include <functional>

static tcp_addr ip_bound(const std::string& ip_string) {
    return tcp_addr::from_string(ip_string);
}

namespace service {

    std::shared_ptr<ServerStreamer> ServerStreamer::Create(const ServerStreamerConfig &config) {
        auto camera_streamer = std::make_shared<ServerStreamer>(config);
        camera_streamer->initialize();
        return camera_streamer;
    }
    ServerStreamer::ServerStreamer(ServerStreamerConfig config):
        _is_started(false), _conf(std::move(config))
    {
        auto assign_strategy = _conf.get_server_client_assignment_strategy();
        if (assign_strategy == ClientAssignmentStrategy::CAMERA_THEN_HEADSET) {
            _connection_assignment_strategy = [this](const tcp_addr &addr) {
                return ConnectionAssignCameraThenHeadset(addr);
            };
        } else if (assign_strategy == ClientAssignmentStrategy::IP_BOUNDS) {
            _connection_assignment_strategy = [this](const tcp_addr &addr) {
                return ConnectionAssignIpBounds(addr);
            };
        } else {
            throw std::runtime_error("Unknown server camera assignment strategy");
        }

        auto switch_strategy = _conf.get_server_camera_switching_strategy();
        if (switch_strategy == CameraSwitchingStrategy::MANUAL_ENTRY) {
            auto self(shared_from_this());
            _camera_switching_strategy = [this, self]() mutable {
                CameraSwitchingManualEntry();
            };
        } else if (switch_strategy == CameraSwitchingStrategy::AUTOMATIC_TIMER) {
            throw std::runtime_error("Switch strategy automatic_timer is not yet implemented");
        } else if (switch_strategy == CameraSwitchingStrategy::HEADSET_CONTROLLED) {
            throw std::runtime_error("Switch strategy headset_controlled is not yet implemented");
        } else if (switch_strategy != CameraSwitchingStrategy::NONE) {
            throw std::runtime_error("Unknown server camera switching strategy");
        }
    }

    void ServerStreamer::initialize() {
        _tcp_context = infrastructure::TcpContext::Create(_conf);
        auto self(shared_from_this());
        _tcp_server = infrastructure::TcpServer::Create(_conf, _tcp_context->GetContext(), self);
    }

    void ServerStreamer::Start() {
        if (_is_started) {
            return;
        }
        /* startup worker thread */
        _work_stop = false;
        if (_camera_switching_strategy) {

            _work_thread = std::make_unique<std::thread>(_camera_switching_strategy);
        }
        /* startup server */
        _tcp_context->Start();
        _tcp_server->Start();
        _is_started = true;
    }

    infrastructure::TcpConnectionType ServerStreamer::ConnectionAssignCameraThenHeadset(const tcp_addr &addr) {
        bool camera_is_connected;
        {
            std::unique_lock lk(_camera_mutex);
            camera_is_connected = !_camera_sessions.empty();
        }
        if (!camera_is_connected) {
            return infrastructure::TcpConnectionType::CAMERA_CONNECTION;
        }
        bool headset_is_connected;
        {
            std::unique_lock lk(_headset_mutex);
            headset_is_connected = !_headset_sessions.empty();
        }
        if (!headset_is_connected) {
            return infrastructure::TcpConnectionType::HEADSET_CONNECTION;
        }
        return infrastructure::TcpConnectionType::UNKNOWN_CONNECTION;
    }

    infrastructure::TcpConnectionType ServerStreamer::ConnectionAssignIpBounds(const tcp_addr &addr) {
        static const tcp_addr min_headset_address = ip_bound("69.4.20.100");
        static const tcp_addr min_camera_address = ip_bound("69.4.20.200");
        if (addr >= min_camera_address) {
            return infrastructure::TcpConnectionType::CAMERA_CONNECTION;
        } else if (addr >= min_headset_address) {
            return infrastructure::TcpConnectionType::HEADSET_CONNECTION;
        }
        return infrastructure::TcpConnectionType::UNKNOWN_CONNECTION;
    }

    infrastructure::TcpConnectionType ServerStreamer::GetConnectionType(const tcp_addr &addr) {
        return _connection_assignment_strategy(addr);
    }

    void ServerStreamer::CameraSwitchingManualEntry() {
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
                    std::cout << "Invalid input. Please enter 200 or 255." << std::endl;
                } else if (200 <= value && value <= 255) {
                    auto c_value = static_cast<unsigned char>(value);
                    std::array<unsigned char, 4> bytes = {69, 4, 20, c_value};
                    tcp_addr use_addr(bytes);
                    std::unique_lock<std::mutex> lock(_camera_mutex);
                    const auto &find_session = _camera_sessions.find(use_addr);
                    if (find_session != _camera_sessions.end()) {
                        std::cout << "Switching to camera: " <<  value << std::endl;
                        _connection_manager.ChangeAllMappings(find_session->first);
                    } else {
                        std::cout << "Camera " << value << " is unavailable" << std::endl;
                    }
                } else {
                    std::cout << "Invalid input. Please enter 200 or 255." << std::endl;
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

    void ServerStreamer::CameraSwitchingAutomaticTimer() {
        // todo: implement
    }

    void ServerStreamer::CameraSwitchingCameraControlled() {
        // todo: implement
    }

    unsigned long ServerStreamer::CreateCameraServerConnection(
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

    void ServerStreamer::PostCameraServerBuffer(const tcp_addr &addr, std::shared_ptr<ResizableBuffer> &&buffer) {
        _connection_manager.PostMessage(addr, std::move(buffer));
    }

    void ServerStreamer::DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> camera_session) {

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

    unsigned long ServerStreamer::CreateHeadsetServerConnection(
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

    void ServerStreamer::DestroyHeadsetServerConnection(
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

    void ServerStreamer::Stop() {
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
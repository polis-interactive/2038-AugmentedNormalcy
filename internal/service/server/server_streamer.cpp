//
// Created by brucegoose on 4/8/23.
//

#include <functional>

#include "domain/headset_domain.hpp"

#include "server_streamer.hpp"


typedef std::chrono::high_resolution_clock Clock;
using namespace std::literals;

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
    {}

    void ServerStreamer::assignStrategies() {
        auto self(shared_from_this());

        auto assign_strategy = _conf.get_server_client_assignment_strategy();
        if (assign_strategy == ClientAssignmentStrategy::CAMERA_THEN_HEADSET) {
            _connection_assignment_strategy = [this, self](const tcp::endpoint &endpoint) {
                return ConnectionAssignCameraThenHeadset(endpoint);
            };
        } else if (assign_strategy == ClientAssignmentStrategy::IP_BOUNDS) {
            _connection_assignment_strategy = [this, self](const tcp::endpoint &endpoint) {
                return ConnectionAssignIpBounds(endpoint);
            };
        } else if (assign_strategy == ClientAssignmentStrategy::ENDPOINT_PORT) {
            _connection_assignment_strategy = [this, self](const tcp::endpoint &endpoint) {
                return ConnectionAssignEndpointPort(endpoint);
            };
        } else {
            throw std::runtime_error("Unknown server camera assignment strategy");
        }

        auto switch_strategy = _conf.get_server_camera_switching_strategy();
        if (switch_strategy == CameraSwitchingStrategy::MANUAL_ENTRY) {
            _camera_switching_strategy = [this, self]() mutable {
                CameraSwitchingManualEntry();
            };
        } else if (switch_strategy == CameraSwitchingStrategy::AUTOMATIC_TIMER) {
            _camera_switching_strategy = [this, self]() mutable {
                CameraSwitchingAutomaticTimer();
            };
        } else if (switch_strategy == CameraSwitchingStrategy::LOCATION_BASED) {
            throw std::runtime_error("Switch strategy location_based is not yet implemented");
        } else if (switch_strategy == CameraSwitchingStrategy::HEADSET_CONTROLLED){
            _allow_headset_switches = true;
        } else if (switch_strategy != CameraSwitchingStrategy::NONE) {
            throw std::runtime_error("Unknown server camera switching strategy");
        }
    }

    void ServerStreamer::initialize() {
        assignStrategies();
        _asio_context = AsioContext::Create(_conf);
        auto self(shared_from_this());
        _tcp_server = infrastructure::TcpServer::Create(_conf, _asio_context->GetContext(), self);
        _websocket_server = infrastructure::WebsocketServer::Create(_conf, _asio_context->GetContext(), self);
    }

    void ServerStreamer::Start() {
        if (_is_started) {
            return;
        }
        /* startup worker thread */
        _work_stop = false;
        if (_camera_switching_strategy != nullptr) {
            _work_thread = std::make_unique<std::thread>(_camera_switching_strategy);
        }
        /* startup server */
        _asio_context->Start();
        _tcp_server->Start();
        _websocket_server->Start();
        _is_started = true;
    }

    ConnectionType ServerStreamer::ConnectionAssignCameraThenHeadset(const tcp::endpoint &endpoint) {
        const auto connection_counts = _connection_manager.GetConnectionCounts();
        if (connection_counts.first == 0 && connection_counts.second == 0) {
            return ConnectionType::CAMERA_CONNECTION;
        } else if (connection_counts.second == 0) {
            return ConnectionType::HEADSET_CONNECTION;
        } else {
            return ConnectionType::UNKNOWN_CONNECTION;
        }
    }

    ConnectionType ServerStreamer::ConnectionAssignIpBounds(const tcp::endpoint &endpoint) {
        static const tcp_addr min_headset_address = ip_bound("69.4.20.100");
        static const tcp_addr min_display_address = ip_bound("69.4.20.190");
        static const tcp_addr min_camera_address = ip_bound("69.4.20.200");
        const auto &addr = endpoint.address().to_v4();
        if (addr >= min_camera_address) {
            return ConnectionType::CAMERA_CONNECTION;
        } else if (addr >= min_display_address) {
            return ConnectionType::DISPLAY_CONNECTION;
        } else if (addr >= min_headset_address) {
            return ConnectionType::HEADSET_CONNECTION;
        }
        return ConnectionType::UNKNOWN_CONNECTION;
    }

    ConnectionType ServerStreamer::ConnectionAssignEndpointPort(const tcp::endpoint &endpoint) {
        const auto &port = endpoint.port();
        if (port == 11111 || port == 12111) {
            return ConnectionType::CAMERA_CONNECTION;
        } else if (port == 21222 || port == 22222) {
            return ConnectionType::HEADSET_CONNECTION;
        } else if (port == 31333 || port == 32333) {
            return ConnectionType::DISPLAY_CONNECTION;
        } else {
            return ConnectionType::UNKNOWN_CONNECTION;
        }
    }

    ConnectionType ServerStreamer::GetConnectionType(const tcp::endpoint &endpoint) {
        return _connection_assignment_strategy(endpoint);
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
                    const bool did_change = _connection_manager.PointReaderAtWriters(use_addr);
                    if (!did_change) {
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
        const auto automatic_timeout = _conf.get_server_camera_switching_automatic_timeout();
        auto last_timer = Clock::now();
        while(!_work_stop) {
            const auto now = Clock::now();
            const auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_timer);
            if (duration.count() < automatic_timeout) {
                std::this_thread::sleep_for(1s);
                continue;
            }
            auto did_change = _connection_manager.RotateAllConnections();
            if (did_change) {
                std::cout << "ServerStreamer::CameraSwitchingAutomaticTimer Successfully rotated Connections" << std::endl;
            } else {
                std::cout << "ServerStreamer::CameraSwitchingAutomaticTimer Failed to rotate connections" << std::endl;
            }
            last_timer = now;
        }
    }

    unsigned long ServerStreamer::CreateCameraServerConnection(
        std::shared_ptr<infrastructure::TcpSession> &&camera_session
    ) {
        return _connection_manager.AddReaderSession(std::move(camera_session));
    }

    void ServerStreamer::PostCameraServerBuffer(const tcp_addr &addr, std::shared_ptr<ResizableBuffer> &&buffer) {
        _connection_manager.PostMessage(addr, std::move(buffer));
    }

    void ServerStreamer::DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> &&camera_session) {
        std::cout << "ServerStreamer::DestroyCameraServerConnection trying" << std::endl;
        _connection_manager.RemoveReaderSession(std::move(camera_session));
        std::cout << "ServerStreamer::DestroyCameraServerConnection finished" << std::endl;
    }

    unsigned long ServerStreamer::CreateHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> &&headset_session
    ) {
        return _connection_manager.AddWriterSession(std::move(headset_session));
    }

    void ServerStreamer::DestroyHeadsetServerConnection(
        std::shared_ptr<infrastructure::WritableTcpSession> &&headset_session
    ) {
        return _connection_manager.RemoveWriterSession(std::move(headset_session));
    }

    bool ServerStreamer::PostWebsocketMessage(
        const ConnectionType connection_type, const tcp_addr addr, nlohmann::json &&message
    ) {
        auto domain_message = domain::DomainMessage::TryParseMessage(std::move(message));
        if (domain_message == nullptr) {
            return false;
        }
        switch (const auto message_type = domain_message->GetMessageType(); message_type) {
            case domain::DomainMessage::RotateCamera:
                if (connection_type == ConnectionType::CAMERA_CONNECTION) {
                    std::cout << "ServerStreamer::PostWebsocketMessage cameras can't can call RotateCamera"
                        << std::endl;
                    return false;
                }
                _connection_manager.RotateWriterConnection(addr);
                return true;
            default:
                std::cout << "ServerStreamer::PostWebsocketMessage unhandled domain message type: "
                    << message_type << std::endl;
                return false;
        }
    }

    void ServerStreamer::Stop() {
        if (!_is_started) {
            return;
        }
        /* cleanup connections */
        _connection_manager.Clear();

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
        _websocket_server->Stop();
        _tcp_server->Stop();
        _asio_context->Stop();
        _is_started = false;
    }
}
//
// Created by brucegoose on 4/8/23.
//

#ifndef AUGMENTEDNORMALCY_SERVICE_SERVER_STREAMER_HPP
#define AUGMENTEDNORMALCY_SERVICE_SERVER_STREAMER_HPP

#include "connection_manager.hpp"

#include "infrastructure/tcp/tcp_context.hpp"


namespace service {

    enum class ClientAssignmentStrategy {
        CAMERA_THEN_HEADSET,
        IP_BOUNDS,
        ENDPOINT_PORT,
    };

    enum class CameraSwitchingStrategy {
        MANUAL_ENTRY,
        AUTOMATIC_TIMER,
        HEADSET_CONTROLLED,
        NONE
    };

    struct ServerStreamerConfig :
            public infrastructure::TcpContextConfig,
            public infrastructure::TcpServerConfig
    {
        ServerStreamerConfig(
            int tcp_pool_size, int tcp_server_port, int tcp_server_timeout_on_read,
            int camera_buffer_count, int headset_buffer_count, int buffer_size,
            ClientAssignmentStrategy assign_strategy, CameraSwitchingStrategy switch_strategy,
            int switch_automatic_timeout
        ):
                _tcp_pool_size(tcp_pool_size),
                _tcp_server_port(tcp_server_port),
                _tcp_server_timeout_on_read(tcp_server_timeout_on_read),
                _camera_buffer_count(camera_buffer_count),
                _headset_buffer_count(headset_buffer_count),
                _buffer_size(buffer_size),
                _assign_strategy(assign_strategy),
                _switch_strategy(switch_strategy),
                _switch_automatic_timeout(switch_automatic_timeout)
        {}

        [[nodiscard]] int get_tcp_pool_size() const override {
            return _tcp_pool_size;
        };

        [[nodiscard]] int get_tcp_server_port() const override {
            return _tcp_server_port;
        }
        [[nodiscard]] int get_tcp_camera_session_buffer_count() const override {
            return _camera_buffer_count;
        }

        [[nodiscard]] virtual int get_tcp_headset_session_buffer_count() const override {
            return _headset_buffer_count;
        };

        [[nodiscard]] int get_tcp_server_buffer_size() const override {
            return _buffer_size;
        }

        [[nodiscard]] int get_tcp_server_timeout_on_read() const override {
            return _tcp_server_timeout_on_read;
        }
        [[nodiscard]] ClientAssignmentStrategy get_server_client_assignment_strategy() const {
            return _assign_strategy;
        }
        [[nodiscard]] CameraSwitchingStrategy get_server_camera_switching_strategy() const {
            return _switch_strategy;
        }
        [[nodiscard]] int get_server_camera_switching_automatic_timeout() const {
            return _switch_automatic_timeout;
        }
    private:
        const int _tcp_pool_size;
        const int _tcp_server_port;
        const int _tcp_server_timeout_on_read;
        const int _camera_buffer_count;
        const int _headset_buffer_count;
        const int _buffer_size;
        const ClientAssignmentStrategy _assign_strategy;
        const CameraSwitchingStrategy _switch_strategy;
        const int _switch_automatic_timeout;
    };



    class ServerStreamer:
        public std::enable_shared_from_this<ServerStreamer>,
        public infrastructure::TcpServerManager
    {
    public:
        static std::shared_ptr<ServerStreamer> Create(const ServerStreamerConfig &config);
        explicit ServerStreamer(ServerStreamerConfig config);
        void Start();
        void Stop();
        void Unset() {
            _tcp_server.reset();
            _tcp_context.reset();
        }
        // tcp server
        [[nodiscard]] infrastructure::TcpConnectionType GetConnectionType(const tcp::endpoint &endpoint) override;

        // camera session
        [[nodiscard]]  unsigned long CreateCameraServerConnection(
            std::shared_ptr<infrastructure::TcpSession> &&camera_session
        ) override;
        void PostCameraServerBuffer(const tcp_addr &addr, std::shared_ptr<ResizableBuffer> &&buffer) override;
        void DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> &&camera_session) override;

        // headset session
        [[nodiscard]] unsigned long CreateHeadsetServerConnection(
            std::shared_ptr<infrastructure::WritableTcpSession> &&headset_session
        ) override;
        void DestroyHeadsetServerConnection(
            std::shared_ptr<infrastructure::WritableTcpSession> &&headset_session
        ) override;

    private:
        void assignStrategies();
        void initialize();

        [[nodiscard]] infrastructure::TcpConnectionType ConnectionAssignCameraThenHeadset(const tcp::endpoint &endpoint);
        [[nodiscard]] infrastructure::TcpConnectionType ConnectionAssignEndpointPort(const tcp::endpoint &endpoint);
        [[nodiscard]] infrastructure::TcpConnectionType ConnectionAssignIpBounds(const tcp::endpoint &endpoint);
        std::function<infrastructure::TcpConnectionType(const tcp::endpoint &)> _connection_assignment_strategy;

        ServerStreamerConfig _conf;
        std::atomic_bool _is_started = false;
        std::shared_ptr<infrastructure::TcpContext> _tcp_context = nullptr;
        std::shared_ptr<infrastructure::TcpServer> _tcp_server = nullptr;

        ConnectionManager _connection_manager;

        void CameraSwitchingManualEntry();
        void CameraSwitchingAutomaticTimer();
        void CameraSwitchingHeadsetControlled();
        std::function<void(void)> _camera_switching_strategy = nullptr;
        std::unique_ptr<std::thread> _work_thread;
        std::atomic<bool> _work_stop = { true };
    };
}

#endif //AUGMENTEDNORMALCY_SERVICE_SERVER_STREAMER_HPP

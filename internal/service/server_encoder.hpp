//
// Created by brucegoose on 4/8/23.
//

#ifndef AUGMENTEDNORMALCY_SERVICE_SERVER_ENCODER_HPP
#define AUGMENTEDNORMALCY_SERVICE_SERVER_ENCODER_HPP

#include <boost/asio/ip/tcp.hpp>
#include <utility>

#include "infrastructure/tcp/tcp_context.hpp"
#include "infrastructure/tcp/tcp_server.hpp"
#include "infrastructure/encoder/jetson_encoder.hpp"

using boost::asio::ip::tcp;
typedef boost::asio::ip::address_v4 tcp_addr;

typedef std::chrono::steady_clock Clock;

namespace service {
    struct ServerEncoderConfig:
        public infrastructure::TcpContextConfig,
        public infrastructure::TcpServerConfig,
        public infrastructure::EncoderConfig
    {
        ServerEncoderConfig(
            int tcp_pool_size, int tcp_server_port, int upstream_buffer_count, int downstream_buffer_count
        ):
            _tcp_pool_size(tcp_pool_size),
            _tcp_server_port(tcp_server_port),
            _upstream_buffer_count(upstream_buffer_count),
            _downstream_buffer_count(downstream_buffer_count)
        {}
        [[nodiscard]] int get_tcp_pool_size() const override {
            return _tcp_pool_size;
        };
        [[nodiscard]] int get_tcp_server_port() const override {
            return _tcp_server_port;
        }
        [[nodiscard]] unsigned int get_encoder_upstream_buffer_count() const override {
            return _upstream_buffer_count;
        }
        [[nodiscard]] unsigned int get_encoder_downstream_buffer_count() const override {
            return _downstream_buffer_count;
        }
        [[nodiscard]] std::pair<int, int> get_encoder_width_height() const override {
            return { 1536, 864 };
        }
    private:
        const int _tcp_pool_size;
        const int _tcp_server_port;
        const int _upstream_buffer_count;
        const int _downstream_buffer_count;
    };

    typedef std::pair<tcp::endpoint , std::shared_ptr<SizedBuffer>> InputBuffer;

    class ServerEncoder:
        public std::enable_shared_from_this<ServerEncoder>,
        public infrastructure::TcpServerManager
    {
    public:
        static std::shared_ptr<ServerEncoder> Create(const ServerEncoderConfig &config);
        explicit ServerEncoder(ServerEncoderConfig config): _is_started(false), _conf(std::move(config)) {}
        void Start() {
            if (_is_started) {
                return;
            }
            /* startup worker thread */
            {
                std::unique_lock<std::mutex> lock(_work_mutex);
                _work_queue = {};
            }
            _work_stop = false;
            auto self(shared_from_this());
            _work_thread = std::make_unique<std::thread>([this, s = std::move(self)]() mutable {
                run();
            });
            /* startup server */
            _tcp_context->Start();
            _tcp_server->Start();
            _is_started = true;
        }
        void Stop() {
            if (!_is_started) {
                return;
            }
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
                    {
                        std::unique_lock<std::mutex> lock(_work_mutex);
                        _work_stop = true;
                        _work_cv.notify_one();
                    }
                    _work_thread->join();
                }
                _work_thread.reset();
            }
            /* stop server */
            _tcp_server->Stop();
            _tcp_context->Stop();
            _is_started = false;
        }
        void Unset() {
            _tcp_server.reset();
            _tcp_context.reset();
        }
        // tcp server
        [[nodiscard]] infrastructure::TcpConnectionType GetConnectionType(tcp::endpoint endpoint) override;
        // camera session
        [[nodiscard]]  infrastructure::CameraConnectionPayload CreateCameraServerConnection(
            std::shared_ptr<infrastructure::TcpSession> camera_session
        ) override;
        void DestroyCameraServerConnection(std::shared_ptr<infrastructure::TcpSession> camera_session) override;

        // headset session
        [[nodiscard]] unsigned long CreateHeadsetServerConnection(
            std::shared_ptr<infrastructure::WritableTcpSession> headset_session
        ) override;
        void DestroyHeadsetServerConnection(
            std::shared_ptr<infrastructure::WritableTcpSession> headset_session
        ) override;
    private:
        void initialize();
        ServerEncoderConfig _conf;
        std::atomic_bool _is_started = false;
        std::shared_ptr<infrastructure::TcpContext> _tcp_context = nullptr;
        std::shared_ptr<infrastructure::TcpServer> _tcp_server = nullptr;

        std::map<tcp::endpoint, std::shared_ptr<infrastructure::TcpSession>> _camera_sessions;
        std::mutex _camera_mutex;
        std::map<tcp::endpoint, std::shared_ptr<infrastructure::WritableTcpSession>> _headset_sessions;
        std::mutex _headset_mutex;
        std::atomic<unsigned long> _last_session_number = { 0 };

        void run();
        void shipBuffer(InputBuffer &&buffer);
        std::unique_ptr<std::thread> _work_thread;
        std::atomic<bool> _work_stop = { true };
        std::mutex _work_mutex;
        std::condition_variable _work_cv;
        std::queue<InputBuffer> _work_queue;
        bool _has_processed;
        tcp::endpoint _last_endpoint;
        std::chrono::time_point<Clock> _last_camera_swap;

    };
}

#endif //AUGMENTEDNORMALCY_SERVICE_SERVER_ENCODER_HPP

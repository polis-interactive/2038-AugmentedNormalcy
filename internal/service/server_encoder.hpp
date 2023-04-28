//
// Created by brucegoose on 4/8/23.
//

#ifndef AUGMENTEDNORMALCY_SERVICE_SERVER_ENCODER_HPP
#define AUGMENTEDNORMALCY_SERVICE_SERVER_ENCODER_HPP

#include <boost/asio/ip/tcp.hpp>
#include <utility>
#include <shared_mutex>
#include <unordered_set>
#include <memory>

#include "infrastructure/tcp/tcp_context.hpp"
#include "infrastructure/tcp/tcp_server.hpp"
#include "infrastructure/encoder/jetson_encoder.hpp"

using boost::asio::ip::tcp;
typedef boost::asio::ip::address_v4 tcp_addr;

typedef std::chrono::steady_clock SteadyClock;

namespace service {
    struct ServerEncoderConfig :
            public infrastructure::TcpContextConfig,
            public infrastructure::TcpServerConfig,
            public infrastructure::EncoderConfig {
        ServerEncoderConfig(
                int tcp_pool_size, int tcp_server_port, int upstream_buffer_count, int downstream_buffer_count
        ) :
                _tcp_pool_size(tcp_pool_size),
                _tcp_server_port(tcp_server_port),
                _upstream_buffer_count(upstream_buffer_count),
                _downstream_buffer_count(downstream_buffer_count) {}

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
            return {1536, 864};
        }

    private:
        const int _tcp_pool_size;
        const int _tcp_server_port;
        const int _upstream_buffer_count;
        const int _downstream_buffer_count;
    };


    class SessionHolder {
    public:
        SessionHolder(
            const tcp::endpoint &endpoint, std::shared_ptr<infrastructure::WritableTcpSession> &&session
        ) : _endpoint(endpoint), _session(std::move(session)) {}

        [[nodiscard]] tcp::endpoint GetEndpoint() const {
            return _endpoint;
        }

        [[nodiscard]] std::shared_ptr<infrastructure::WritableTcpSession> GetTcpSession() const {
            return _session;
        }

        void ReplaceSession(std::shared_ptr<infrastructure::WritableTcpSession> &&session) {
            _session = std::move(session);
        }

        bool operator==(const SessionHolder& other) const {
            return _endpoint == other._endpoint;
        }
        bool operator==(const tcp::endpoint &endpoint) const {
            return _endpoint == endpoint;
        }

    private:
        const tcp::endpoint _endpoint;
        std::shared_ptr<infrastructure::WritableTcpSession> _session;
    };

};

template<>
struct std::hash<service::SessionHolder> {
    std::size_t operator()(const service::SessionHolder& holder) const {
        return std::hash<tcp::endpoint>{}(holder.GetEndpoint());
    }
};

namespace service {

    typedef std::map<tcp::endpoint, std::shared_ptr<infrastructure::WritableTcpSession>> HeadsetSessions;

    class ConnectionManager {
        /*
         * As is, we need to decouple tcp_sessions more; need to be okay with nullptr session, as connection
         * shouldn't care if anything is actually there to connect
         */
    public:
        void AddMapping(
            const tcp::endpoint& camera, const tcp::endpoint& headset,
            std::shared_ptr<infrastructure::WritableTcpSession> session
        ) {
            std::unique_lock lk(_connection_mutex);
            _camera_to_headset[camera].emplace(headset, std::move(session));
            _headset_to_camera[headset] = camera;
        }

        void AddManyMappings(
                const tcp::endpoint &camera,
                const HeadsetSessions &headset_sessions
        ) {
            std::unique_lock lk(_connection_mutex);
            for (const auto &headset_session : headset_sessions) {
                const auto &headset = headset_session.first;
                auto session = headset_session.second;
                _camera_to_headset[camera].emplace(headset, std::move(session));
                _headset_to_camera[headset] = camera;
            }
        }

        void ChangeMapping(const tcp::endpoint &camera, const tcp::endpoint &headset) {
            std::unique_lock lk(_connection_mutex);
            auto old_binding = _headset_to_camera.find(headset);
            if (old_binding == _headset_to_camera.end()) return;

            auto &old_sessions = _camera_to_headset[old_binding->second];
            auto &new_sessions = _camera_to_headset[camera];

            auto session = std::find(old_sessions.begin(), old_sessions.end(), headset);

            new_sessions.emplace(*session);
            old_sessions.erase(session);
            old_binding->second = camera;
        }
        void ChangeAllMappings(const tcp::endpoint &camera) {
            std::unique_lock lk(_connection_mutex);
            auto new_camera_iter = _camera_to_headset.find(camera);
            if (new_camera_iter == _camera_to_headset.end()) {
                new_camera_iter = _camera_to_headset.insert({ camera, {} }).first;
            }
            for (auto old_camera_iter = _camera_to_headset.begin(); old_camera_iter != _camera_to_headset.end();) {
                if (old_camera_iter->first == camera) {
                    ++old_camera_iter;
                    continue;
                }
                auto &old_sessions = old_camera_iter->second;
                auto &new_sessions = new_camera_iter->second;
                for (auto session_iter = old_sessions.begin(); session_iter != old_sessions.end();) {
                    new_sessions.insert(std::move(*session_iter));
                    _headset_to_camera[session_iter->GetEndpoint()] = camera;
                    session_iter = old_sessions.erase(session_iter);
                }
                old_camera_iter = _camera_to_headset.erase(old_camera_iter);
            }
        }

        bool TryReplaceSession(
                const tcp::endpoint &headset, std::shared_ptr<infrastructure::WritableTcpSession> new_session
        ) {
            std::unique_lock lk(_connection_mutex);
            auto binding = _headset_to_camera.find(headset);
            if (binding == _headset_to_camera.end()) return false;

            auto &sessions = _camera_to_headset[binding->second];
            auto session = std::find(sessions.begin(), sessions.end(), headset);
            if (session != sessions.end()) {
                sessions.erase(session);
            }
            sessions.emplace(headset, std::move(new_session));
            return true;
        }

        void PostMessage(const tcp::endpoint &camera, std::shared_ptr<SizedBuffer> &&buffer) {
            std::shared_lock lk(_connection_mutex);
            auto connection = _camera_to_headset.find(camera);
            if (connection == _camera_to_headset.end()) return;

            for (const auto &session : connection->second) {
                const auto &tcp_session = session.GetTcpSession();
                if (tcp_session != nullptr) {
                    auto buffer_copy = buffer;
                    tcp_session->Write(std::move(buffer_copy));
                }
            }
        }

        void RemoveCamera(const tcp::endpoint &camera) {
            std::unique_lock lk(_connection_mutex);
            auto session_set = _camera_to_headset.find(camera);
            if (session_set == _camera_to_headset.end()) return;

            for (const auto &session : session_set->second) {
                auto headset_it = _headset_to_camera.find(session.GetEndpoint());
                if (headset_it != _headset_to_camera.end()) {
                    _headset_to_camera.erase(headset_it);
                }
            }
            _camera_to_headset.erase(session_set);
        }

        void RemoveHeadset(const tcp::endpoint &headset) {
            std::unique_lock lk(_connection_mutex);
            auto connection = _headset_to_camera.find(headset);
            if (connection == _headset_to_camera.end()) return;

            auto camera = _camera_to_headset.find(connection->second);
            if (camera != _camera_to_headset.end()) {
                auto &session_set = camera->second;
                auto binding = std::find(session_set.begin(), session_set.end(), headset);
                if (binding == session_set.end()) {
                    session_set.erase(binding);
                }
            }
            _headset_to_camera.erase(connection);
        }

        void Clear() {
            std::unique_lock lk(_connection_mutex);
            _camera_to_headset.clear();
            _headset_to_camera.clear();
        }

    private:
        mutable std::shared_mutex _connection_mutex;
        std::map<tcp::endpoint, std::unordered_set<SessionHolder>> _camera_to_headset;
        std::map<tcp::endpoint, tcp::endpoint> _headset_to_camera;
    };

    class ServerEncoder:
        public std::enable_shared_from_this<ServerEncoder>,
        public infrastructure::TcpServerManager
    {
    public:
        static std::shared_ptr<ServerEncoder> Create(const ServerEncoderConfig &config);
        explicit ServerEncoder(ServerEncoderConfig config): _is_started(false), _conf(std::move(config)) {}
        void Start();
        void Stop();
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

        ConnectionManager _connection_manager;

        void run();
        std::unique_ptr<std::thread> _work_thread;
        std::atomic<bool> _work_stop = { true };
        bool _has_processed;
        tcp::endpoint _last_endpoint;
        std::chrono::time_point<SteadyClock> _last_camera_swap;

    };
}

#endif //AUGMENTEDNORMALCY_SERVICE_SERVER_ENCODER_HPP

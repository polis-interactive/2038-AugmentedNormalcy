//
// Created by brucegoose on 3/14/23.
//

#include <doctest.h>
#include <iostream>
#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;

#include "infrastructure/tcp/tcp_server.hpp"

struct TestServerConfig:
    public infrastructure::TcpContextConfig,
    public infrastructure::TcpServerConfig
{
    explicit TestServerConfig(int pool_size, int tcp_server_port):
            _pool_size(pool_size),
            _tcp_server_port(tcp_server_port)
    {}
    const int _pool_size;
    const int _tcp_server_port;
    [[nodiscard]] int get_tcp_pool_size() const override {
        return _pool_size;
    };
    [[nodiscard]] int get_tcp_server_port() const override {
        return _tcp_server_port;
    };
};

/* Let's just bring up and shutdown the server */

class NoSessionManager: public infrastructure::TcpServerManager {
public:
    [[nodiscard]] infrastructure::TcpConnectionType GetConnectionType(tcp::endpoint endpoint) override {
        return infrastructure::TcpConnectionType::UNKNOWN_CONNECTION;
    }
    /* dummy's for the rest */
    [[nodiscard]] infrastructure::CameraConnectionPayload CreateCameraConnection(tcp::endpoint endpoint) override {
        return {};
    }
    void DestroyCameraConnection(tcp::endpoint endpoint, unsigned long session_id) override {}
    [[nodiscard]] unsigned long CreateHeadsetConnection(
        tcp::endpoint endpoint, infrastructure::HeadsetWriteCall writeCall
    ) {
        return 0;
    }
    void DestroyHeadsetConnection(tcp::endpoint endpoint, unsigned long session_id) override {}
};


TEST_CASE("Server bring up and tear down") {
    TestServerConfig conf(3, 6969);
    infrastructure::TcpContext ctx(conf);
    ctx.Start();
    auto manager = std::make_shared<NoSessionManager>();
    auto srv_manager = std::static_pointer_cast<infrastructure::TcpServerManager>(manager);
    infrastructure::TcpServer srv(conf, ctx.GetContext(), srv_manager);
    srv.Start();
    std::this_thread::sleep_for(1s);
    srv.Stop();
    ctx.Stop();
}
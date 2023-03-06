//
// Created by brucegoose on 3/5/23.
//

#ifndef AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_SERVER_HPP
#define AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_SERVER_HPP

#include <boost/asio.hpp>
#include "tcp_context.hpp"

using boost::asio::ip::tcp;

namespace infrastructure {
    class CameraSession {

    };
    class HeadsetSession {

    };

    struct TcpServerConfig {
        [[nodiscard]] virtual int get_tcp_server_port() const = 0;
    };

    class TcpServer {
    public:
        TcpServer() = delete;
        TcpServer (const TcpServer&) = delete;
        TcpServer& operator= (const TcpServer&) = delete;
        std::shared_ptr<TcpServer> CreateServer(const TcpServerConfig &config);
    private:
        TcpServer(const TcpServerConfig &config);
    };
}

#endif //AUGMENTEDNORMALCY_INFRASTRUCTURE_TCP_SERVER_HPP

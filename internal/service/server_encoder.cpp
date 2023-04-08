//
// Created by brucegoose on 4/8/23.
//

#include "server_encoder.hpp"

namespace service {
    std::shared_ptr<ServerEncoder> ServerEncoder::Create(const service::ServerEncoderConfig &config) {
        auto camera_streamer = std::make_shared<ServerEncoder>();
        camera_streamer->initialize(config);
        return camera_streamer;
    }

    void ServerEncoder::initialize(const ServerEncoderConfig &config) {
        _tcp_context = infrastructure::TcpContext::Create(config);
        auto self(shared_from_this());
        _tcp_server = infrastructure::TcpServer::Create(config, _tcp_context->GetContext(), self);
    }
}
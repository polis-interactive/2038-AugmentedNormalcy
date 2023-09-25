//
// Created by bruce on 12/29/2022.
//

#include "infrastructure/udp/client.hpp"


namespace infrastructure {
    std::shared_ptr<UdpClient> UdpClientPool::GetOrCreateClient(const std::string& address, payload_buffer_pool &pool) {
        udp::endpoint remote(net::ip::address::from_string(address), _port);
        std::scoped_lock<std::mutex> lk(_mutex);
        if (auto search = _clients.find(remote); search != _clients.end()) {
            auto &[_, client] = *search;
            return client;
        } else {
            auto client = std::make_shared<UdpClient>(_context, remote, pool);
            _clients.try_emplace(remote, client);
            return client;
        }
    }
    void UdpClientPool::Stop() {
        for (auto & [_, client] : _clients) {
            client->Stop();
        }
        _clients.clear();
    }
}
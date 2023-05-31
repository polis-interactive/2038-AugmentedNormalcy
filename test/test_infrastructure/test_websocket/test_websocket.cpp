//
// Created by brucegoose on 5/31/23.
//

#include <doctest.h>

#include "utils/clock.hpp"

#include "test_websocket.hpp"

TEST_CASE("INFRASTRUCTURE_WEBSOCKET-Server_Setup_And_Teardown") {
    const TestServerClientConfig conf;
    auto ctx = AsioContext::Create(conf);
    ctx->Start();
    auto srv_manager = std::make_shared<WebsocketServerManager>([](nlohmann::json &&){});
    std::chrono::time_point< std::chrono::high_resolution_clock> t1, t2, t3, t4, t5;

    {
        t1 = Clock::now();
        auto srv = infrastructure::WebsocketServer::Create(conf, ctx->GetContext(), srv_manager);
        t2 = Clock::now();
        srv->Start();
        t3 = Clock::now();
        srv->Stop();
        t4 = Clock::now();
    }
    t5 = Clock::now();
    auto d1 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
    auto d2 = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2);
    auto d3 = std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3);
    auto d4 = std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4);
    std::cout << "test_websocket/test_weebsocket_server Setup and Teardown: microseconds: " <<
              d1.count() << ", " << d2.count() << ", " << d3.count() << d4.count() << std::endl;
}

TEST_CASE("INFRASTRUCTURE_WEBSOCKET-Client_Setup_And_Teardown") {
    const TestServerClientConfig conf;
    auto ctx = AsioContext::Create(conf);
    ctx->Start();
    auto cln_manager = std::make_shared<WebsocketClientManager>([](nlohmann::json &&){});
    std::chrono::time_point< std::chrono::high_resolution_clock> t1, t2, t3, t4, t5;

    {
        t1 = Clock::now();
        auto cln = infrastructure::WebsocketClient::Create(conf, ctx->GetContext(), cln_manager);
        t2 = Clock::now();
        cln->Start();
        t3 = Clock::now();
        cln->Stop();
        t4 = Clock::now();
    }
    t5 = Clock::now();
    auto d1 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
    auto d2 = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2);
    auto d3 = std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3);
    auto d4 = std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4);
    std::cout << "test_websocket/test_weebsocket_client Setup and Teardown: microseconds: " <<
              d1.count() << ", " << d2.count() << ", " << d3.count() << d4.count() << std::endl;
}
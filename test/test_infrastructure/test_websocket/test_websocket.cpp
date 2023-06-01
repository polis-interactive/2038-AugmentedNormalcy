//
// Created by brucegoose on 5/31/23.
//

#include <doctest.h>

#include "utils/clock.hpp"

#include "test_websocket.hpp"

using namespace nlohmann::literals;

TEST_CASE("INFRASTRUCTURE_WEBSOCKET-Server_Setup_And_Teardown") {
    const TestServerClientConfig conf;
    auto ctx = AsioContext::Create(conf);
    ctx->Start();
    auto srv_manager = std::make_shared<WebsocketClientServerManager>(
        [](nlohmann::json &&){},
        [](nlohmann::json &&){}
    );
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
    auto cln_manager = std::make_shared<WebsocketClientServerManager>(
        [](nlohmann::json &&){},
        [](nlohmann::json &&){}
    );
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

TEST_CASE("INFRASTRUCTURE_WEBSOCKET-Connection") {
    const TestServerClientConfig conf;
    auto ctx = AsioContext::Create(conf);
    ctx->Start();

    auto manager = std::make_shared<WebsocketClientServerManager>(
        [](nlohmann::json &&){}, // server
        [](nlohmann::json &&){} // client
    );

    auto cln = infrastructure::WebsocketClient::Create(conf, ctx->GetContext(), manager);
    cln->Start();
    auto srv = infrastructure::WebsocketServer::Create(conf, ctx->GetContext(), manager);
    srv->Start();
    std::this_thread::sleep_for(3s);
    REQUIRE((manager->_client_is_connected == true));
    std::this_thread::sleep_for(3s);
    REQUIRE((manager->_client_is_connected == true));
    cln->Stop();
    cln->Start();
    std::this_thread::sleep_for(3s);
    REQUIRE((manager->_client_is_connected == true));
    srv->Stop();
    srv->Start();
    std::this_thread::sleep_for(3s);
    REQUIRE((manager->_client_is_connected == true));

    cln->Stop();
    srv->Stop();
    ctx->Stop();
}

TEST_CASE("INFRASTRUCTURE_WEBSOCKET-Client_To_Server") {
    const TestServerClientConfig conf;
    auto ctx = AsioContext::Create(conf);
    ctx->Start();

    std::vector<nlohmann::json> samples {
        R"({
            "message": "foo",
            "payload": true
        })"_json,
        R"({
            "message": "bar",
            "payload": "hello"
        })"_json,
        R"({
            "message": "baz",
            "payload": 1
        })"_json,
        R"({
            "message": "bux",
            "payload": [1, 2, 3, 4]
        })"_json,
        R"({
            "message": "bun",
            "payload": [ "one", "two", "three", "four" ]
        })"_json,
    };
    sort(samples.begin(), samples.end());
    std::vector<nlohmann::json> output {};

    auto on_receive = [&output](nlohmann::json && message) {
        std::cout << "received buffer: " << message << std::endl;
        output.push_back(std::move(message));
    };

    auto manager = std::make_shared<WebsocketClientServerManager>(
        on_receive, // server
        [](nlohmann::json &&){} // client
    );
    auto cln = infrastructure::WebsocketClient::Create(conf, ctx->GetContext(), manager);
    cln->Start();
    auto srv = infrastructure::WebsocketServer::Create(conf, ctx->GetContext(), manager);
    srv->Start();
    std::this_thread::sleep_for(3s);
    REQUIRE((manager->_client_is_connected == true));

    for (auto sample : samples) {
        std::cout << "Writing buffer: " << sample << std::endl;
        cln->PostWebsocketClientMessage(std::move(sample));
        std::this_thread::sleep_for(1ms);
    }
    std::this_thread::sleep_for(100ms);

    // make sure we got them out
    REQUIRE_EQ(samples.size(), output.size());
    // make sure we got them in the same order
    REQUIRE(std::equal(samples.begin(), samples.end(), output.begin()));

    cln->Stop();
    srv->Stop();
    ctx->Stop();
}

TEST_CASE("INFRASTRUCTURE_WEBSOCKET-Client_To_Server_Stress_test") {
    const TestServerClientConfig conf;
    auto ctx = AsioContext::Create(conf);
    ctx->Start();

    std::chrono::time_point< std::chrono::high_resolution_clock> t1, t2;

    std::atomic_int send_count = { 0 };
    std::atomic_int receive_count = { 0 };

    auto on_receive = [&receive_count, &t2](nlohmann::json && message) {
        receive_count += 1;
        t2 = Clock::now();
    };

    auto manager = std::make_shared<WebsocketClientServerManager>(
            on_receive, // server
            [](nlohmann::json &&){} // client
    );
    auto cln = infrastructure::WebsocketClient::Create(conf, ctx->GetContext(), manager);
    cln->Start();
    auto srv = infrastructure::WebsocketServer::Create(conf, ctx->GetContext(), manager);
    srv->Start();
    std::this_thread::sleep_for(3s);
    REQUIRE((manager->_client_is_connected == true));

    nlohmann::json message = R"({
        "message": "foo",
        "payload": 0
    })"_json;

    t1 = Clock::now();
    for (int i = 0; i < 10000; i++) {
        auto new_message = message;
        new_message["payload"] = i;
        cln->PostWebsocketClientMessage(std::move(new_message));
        send_count += 1;
        std::this_thread::sleep_for(1ms);
    }
    std::this_thread::sleep_for(2s);

    REQUIRE_EQ(send_count, 10000);
    // cut it some slack if it couldn't keep up
    std::cout << receive_count << std::endl;
    REQUIRE_GT(send_count, receive_count - 10);

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    std::cout << "test_infrastructure/test_tcp/communication/push_server sends 10000 messages: " <<
              d1.count() << "ms" << std::endl;

    cln->Stop();
    srv->Stop();
    ctx->Stop();
}
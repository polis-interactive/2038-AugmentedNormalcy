//
// Created by brucegoose on 3/14/23.
//

#include <doctest.h>
#include <iostream>
#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;

#include "server.hpp"
#include "client.hpp"
#include "communication.hpp"

#include "infrastructure/tcp/tcp_server.hpp"


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

TEST_CASE("Client bring up and tear down, camera client") {
    TestClientServerConfig conf(2, 6969, "127.0.0.1", true);
    infrastructure::TcpContext ctx(conf);
    ctx.Start();
    auto manager = std::make_shared<TcpClientManager>(nullptr);
    auto client_manager = std::static_pointer_cast<infrastructure::TcpClientManager>(manager);
    infrastructure::TcpClient client(conf, ctx.GetContext(), client_manager);
    client.Start();
    std::this_thread::sleep_for(1s);
    client.Stop();
    ctx.Stop();
}

TEST_CASE("Client bring up and tear down, headset client") {
    TestClientServerConfig conf(2, 6969, "127.0.0.1", false);
    infrastructure::TcpContext ctx(conf);
    ctx.Start();
    auto manager = std::make_shared<TcpClientManager>(nullptr);
    auto client_manager = std::static_pointer_cast<infrastructure::TcpClientManager>(manager);
    infrastructure::TcpClient client(conf, ctx.GetContext(), client_manager);
    client.Start();
    std::this_thread::sleep_for(1s);
    client.Stop();
    ctx.Stop();
}


TEST_CASE("Push from camera client to server") {
    TestClientServerConfig conf(3, 6969, "127.0.0.1", true);
    infrastructure::TcpContext ctx(conf);
    ctx.Start();

    // setting up an upstream / downstream to send receive values
    // samples must be 5 chars each
    std::vector<std::string> samples { "hello", "world", "bar__", "baz__", "foo__", "bax__" };
    sort(samples.begin(), samples.end());
    std::vector<std::string> output {};
    auto on_receive = [&output](std::shared_ptr<SizedBuffer> &&buffer) {
        auto fake_buffer = std::static_pointer_cast<FakeSizedBuffer>(buffer);
        std::cout << "received buffer: " << fake_buffer->_buffer << std::endl;
        output.emplace_back(fake_buffer->_buffer);
        buffer.reset();
    };
    // 5 buffers total means one needs to be reused
    auto pool = std::make_shared<FakeBufferPool>(on_receive, 5, 5);

    auto manager = std::make_shared<TcpCameraClientServerManager>(pool);

    // just to be cheeky, we are going to start up the client first
    auto client_manager = std::static_pointer_cast<infrastructure::TcpClientManager>(manager);
    infrastructure::TcpClient client(conf, ctx.GetContext(), client_manager);
    client.Start();
    auto srv_manager = std::static_pointer_cast<infrastructure::TcpServerManager>(manager);
    infrastructure::TcpServer srv(conf, ctx.GetContext(), srv_manager);
    srv.Start();
    std::this_thread::sleep_for(3s);
    REQUIRE(manager->ClientIsConnected());

    // start sending the samples
    for (auto &sample : samples) {
        std::cout << "Writing buffer: " << sample << std::endl;
        auto buffer = std::make_shared<FakeSizedBuffer>(sample);
        manager->_write_call(std::move(buffer));
        std::this_thread::sleep_for(1ms);
    }
    std::this_thread::sleep_for(10ms);

    // make sure we got them out
    REQUIRE_EQ(samples.size(), output.size());
    // make sure we got them in the same order
    REQUIRE(std::equal(samples.begin(), samples.end(), output.begin()));
    // make sure we freed them to boot (one should be held by the pool on receive)
    REQUIRE_EQ(pool->AvailableBuffers(), 5 - 1);

    client.Stop();
    srv.Stop();
    ctx.Stop();
}

TEST_CASE("Push from camera client to server, little stressy-er") {
    TestClientServerConfig conf(3, 6969, "127.0.0.1", true);
    infrastructure::TcpContext ctx(conf);
    ctx.Start();

    std::chrono::time_point< std::chrono::high_resolution_clock> t1, t2;

    std::atomic_int send_count = 0;
    std::atomic_int receive_count = 0;
    auto on_receive = [&receive_count, &t2](std::shared_ptr<SizedBuffer> &&buffer) {
        receive_count += 1;
        t2 = Clock::now();
        buffer.reset();
    };
    auto pool = std::make_shared<FakeBufferPool>(on_receive, 10, 5);

    auto manager = std::make_shared<TcpCameraClientServerManager>(pool);

    auto srv_manager = std::static_pointer_cast<infrastructure::TcpServerManager>(manager);
    infrastructure::TcpServer srv(conf, ctx.GetContext(), srv_manager);
    srv.Start();

    auto client_manager = std::static_pointer_cast<infrastructure::TcpClientManager>(manager);
    infrastructure::TcpClient client(conf, ctx.GetContext(), client_manager);
    client.Start();

    std::this_thread::sleep_for(2s);
    REQUIRE(manager->ClientIsConnected());

    t1 = Clock::now();
    for (int i = 0; i < 10000; i++) {
        auto s = std::to_string(i);
        if (s.size() < 10) {
            s.insert(s.begin(), 10 - s.size(), '0');
        }
        auto buffer = std::make_shared<FakeSizedBuffer>(s);
        manager->_write_call(std::move(buffer));
        send_count += 1;
    }
    std::this_thread::sleep_for(1s);

    REQUIRE_EQ(send_count, 10000);
    REQUIRE_EQ(send_count, receive_count);
    REQUIRE_EQ(pool->AvailableBuffers(), 5 - 1);

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    std::cout << "test_infrastructure/test_tcp/communication sends 10000 messages: " <<
        d1.count() << "ms" << std::endl;

    client.Stop();
    srv.Stop();
    ctx.Stop();
}
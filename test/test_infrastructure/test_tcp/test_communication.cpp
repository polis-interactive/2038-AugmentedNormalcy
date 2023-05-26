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
#include "camera.hpp"
#include "headset.hpp"

#include "infrastructure/tcp/tcp_server.hpp"


TEST_CASE("INFRASTRUCTURE_TCP_SERVER-Start-and-stop") {
    TestServerConfig conf(3, 42069);
    auto ctx = infrastructure::TcpContext::Create(conf);
    ctx->Start();
    auto manager = std::make_shared<NoSessionManager>();
    auto srv_manager = std::static_pointer_cast<infrastructure::TcpServerManager>(manager);
    auto srv = infrastructure::TcpServer::Create(conf, ctx->GetContext(), srv_manager);
    srv->Start();
    std::this_thread::sleep_for(1s);
    srv->Stop();
    ctx->Stop();
}

TEST_CASE("INFRASTRUCTURE_TCP_CLIENT-Camera-Start-and-stop") {
    TestClientServerConfig conf(2, 42069, "127.0.0.1", true);
    auto ctx = infrastructure::TcpContext::Create(conf);
    ctx->Start();
    auto manager = std::make_shared<TcpClientManager>();
    auto client_manager = std::static_pointer_cast<infrastructure::TcpClientManager>(manager);
    auto client = infrastructure::TcpClient::Create(conf, ctx->GetContext(), client_manager);
    client->Start();
    std::this_thread::sleep_for(1s);
    client->Stop();
    ctx->Stop();
}

TEST_CASE("INFRASTRUCTURE_TCP_CLIENT-Headset-Start-and-stop") {
    TestClientServerConfig conf(2, 42069, "127.0.0.1", false);
    auto ctx = infrastructure::TcpContext::Create(conf);
    ctx->Start();
    auto manager = std::make_shared<TcpClientManager>();
    auto client_manager = std::static_pointer_cast<infrastructure::TcpClientManager>(manager);
    auto client = infrastructure::TcpClient::Create(conf, ctx->GetContext(), client_manager);
    client->Start();
    std::this_thread::sleep_for(1s);
    client->Stop();
    ctx->Stop();
}


TEST_CASE("INFRASTRUCTURE_TCP-Camera-to-Server") {
    TestClientServerConfig conf(3, 42069, "127.0.0.1", true);
    auto ctx = infrastructure::TcpContext::Create(conf);
    ctx->Start();

    // setting up an upstream / downstream to send receive values
    // samples must be 5 chars each
    std::vector<std::string> samples { "hello", "world", "bar__", "baz__", "foo__", "bax__" };
    sort(samples.begin(), samples.end());
    std::vector<std::string> output {};
    auto on_receive = [&output](std::shared_ptr<ResizableBuffer> &&buffer) {
        std::string s;
        REQUIRE_EQ(buffer->GetSize(), 5);
        s.assign((char *) buffer->GetMemory(), buffer->GetSize());
        std::cout << "received buffer: " << s << std::endl;
        output.push_back(s);
        buffer.reset();
    };

    auto manager = std::make_shared<TcpCameraClientServerManager>(on_receive);

    // just to be cheeky, we are going to start up the client first
    auto client_manager = std::static_pointer_cast<infrastructure::TcpClientManager>(manager);
    auto client = infrastructure::TcpClient::Create(conf, ctx->GetContext(), client_manager);
    client->Start();
    auto srv_manager = std::static_pointer_cast<infrastructure::TcpServerManager>(manager);
    auto srv = infrastructure::TcpServer::Create(conf, ctx->GetContext(), srv_manager);
    srv->Start();
    std::this_thread::sleep_for(3s);
    REQUIRE(manager->ClientIsConnected());

    // start sending the samples
    for (auto &sample : samples) {
        std::cout << "Writing buffer: " << sample << std::endl;
        auto buffer = std::make_shared<FakeSizedBuffer>(sample);
        client->Post(std::move(buffer));
        std::this_thread::sleep_for(1ms);
    }
    std::this_thread::sleep_for(10ms);

    // make sure we got them out
    REQUIRE_EQ(samples.size(), output.size());
    // make sure we got them in the same order
    REQUIRE(std::equal(samples.begin(), samples.end(), output.begin()));

    client->Stop();
    srv->Stop();
    ctx->Stop();
}

TEST_CASE("INFRASTRUCTURE_TCP-Camera-to-Server-Stress") {
    TestClientServerConfig conf(3, 42069, "127.0.0.1", true);
    auto ctx = infrastructure::TcpContext::Create(conf);
    ctx->Start();

    std::chrono::time_point< std::chrono::high_resolution_clock> t1, t2;

    std::atomic_int send_count = 0;
    std::atomic_int receive_count = 0;
    auto on_receive = [&receive_count, &t2](std::shared_ptr<SizedBuffer> &&buffer) {
        receive_count += 1;
        t2 = Clock::now();
        buffer.reset();
    };
    auto manager = std::make_shared<TcpCameraClientServerManager>(on_receive);

    auto srv_manager = std::static_pointer_cast<infrastructure::TcpServerManager>(manager);
    auto srv = infrastructure::TcpServer::Create(conf, ctx->GetContext(), srv_manager);
    srv->Start();

    auto client_manager = std::static_pointer_cast<infrastructure::TcpClientManager>(manager);
    auto client = infrastructure::TcpClient::Create(conf, ctx->GetContext(), client_manager);
    client->Start();

    std::this_thread::sleep_for(2s);
    REQUIRE(manager->ClientIsConnected());

    t1 = Clock::now();
    for (int i = 0; i < 10000; i++) {
        auto s = std::to_string(i);
        if (s.size() < 10) {
            s.insert(s.begin(), 10 - s.size(), '0');
        }
        auto buffer = std::make_shared<FakeSizedBuffer>(s);
        client->Post(std::move(buffer));
        send_count += 1;
    }
    std::this_thread::sleep_for(2s);

    REQUIRE_EQ(send_count, 10000);
    REQUIRE_EQ(send_count, receive_count);

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    std::cout << "test_infrastructure/test_tcp/communication/pull_server sends 10000 messages: " <<
        d1.count() << "ms" << std::endl;

    client->Stop();
    srv->Stop();
    ctx->Stop();
}

TEST_CASE("INFRASTRUCTURE_TCP-Server-to-Headset")  {
    /* this looks really similar, but all the buffers are pushed from server to client in this case */
    TestClientServerConfig conf(3, 42069, "127.0.0.1", false);
    auto ctx = infrastructure::TcpContext::Create(conf);
    ctx->Start();

    // setting up an upstream / downstream to send receive values
    // samples must be 5 chars each
    std::vector<std::string> samples { "hello", "world", "bar__", "baz__", "foo__", "bax__" };
    sort(samples.begin(), samples.end());
    std::vector<std::string> output {};
    auto on_receive = [&output](std::shared_ptr<SizedBuffer> &&buffer) {
        std::string s;
        REQUIRE_EQ(buffer->GetSize(), 5);
        s.assign((char *) buffer->GetMemory(), buffer->GetSize());
        std::cout << "received buffer: " << s << std::endl;
        output.push_back(s);
        buffer.reset();
    };

    auto manager = std::make_shared<TcpHeadsetClientServerManager>(on_receive);

    // just to be cheeky, we are going to start up the client first
    auto client_manager = std::static_pointer_cast<infrastructure::TcpClientManager>(manager);
    auto client = infrastructure::TcpClient::Create(conf, ctx->GetContext(), client_manager);
    client->Start();

    auto srv_manager = std::static_pointer_cast<infrastructure::TcpServerManager>(manager);
    auto srv = infrastructure::TcpServer::Create(conf, ctx->GetContext(), srv_manager);
    srv->Start();
    std::this_thread::sleep_for(3s);
    REQUIRE(manager->ClientIsConnected());

    // start sending the samples
    for (auto &sample : samples) {
        std::cout << "Writing buffer: " << sample << std::endl;
        auto buffer = std::make_shared<FakeSizedBuffer>(sample);
        manager->_session->Write(std::move(buffer));
        std::this_thread::sleep_for(1ms);
    }
    std::this_thread::sleep_for(10ms);

    // make sure we got them out
    REQUIRE_EQ(samples.size(), output.size());
    // make sure we got them in the same order
    REQUIRE(std::equal(samples.begin(), samples.end(), output.begin()));

    client->Stop();
    srv->Stop();
    ctx->Stop();
}

TEST_CASE("INFRASTRUCTURE_TCP-Server-to-Headset-Stress"){
    /* again, looks really similar, but all the buffers are pushed from server to client in this case */
    TestClientServerConfig conf(3, 42069, "127.0.0.1", false);
    auto ctx = infrastructure::TcpContext::Create(conf);
    ctx->Start();

    std::chrono::time_point< std::chrono::high_resolution_clock> t1, t2;

    std::atomic_int send_count = 0;
    std::atomic_int receive_count = 0;
    auto on_receive = [&receive_count, &t2](std::shared_ptr<SizedBuffer> &&buffer) {
        receive_count += 1;
        t2 = Clock::now();
        buffer.reset();
    };

    auto manager = std::make_shared<TcpHeadsetClientServerManager>(on_receive);

    auto srv_manager = std::static_pointer_cast<infrastructure::TcpServerManager>(manager);
    auto srv = infrastructure::TcpServer::Create(conf, ctx->GetContext(), srv_manager);
    srv->Start();

    auto client_manager = std::static_pointer_cast<infrastructure::TcpClientManager>(manager);
    auto client = infrastructure::TcpClient::Create(conf, ctx->GetContext(), client_manager);
    client->Start();

    std::this_thread::sleep_for(3s);
    REQUIRE(manager->ClientIsConnected());

    t1 = Clock::now();
    for (int i = 0; i < 10000; i++) {
        auto s = std::to_string(i);
        if (s.size() < 10) {
            s.insert(s.begin(), 10 - s.size(), '0');
        }
        auto buffer = std::make_shared<FakeSizedBuffer>(s);
        manager->_session->Write(std::move(buffer));
        send_count += 1;
        // since write now posts to an executor, and that involves some mutexes, need the smallest
        // of timeouts to make it work
        std::this_thread::sleep_for(50us);
    }
    std::this_thread::sleep_for(2s);

    REQUIRE_EQ(send_count, 10000);
    // cut it some slack if it couldn't keep up
    std::cout << receive_count << std::endl;
    REQUIRE_GT(send_count, receive_count - 10);

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    std::cout << "test_infrastructure/test_tcp/communication/push_server sends 10000 messages: " <<
              d1.count() << "ms" << std::endl;

    client->Stop();
    srv->Stop();
    ctx->Stop();
}
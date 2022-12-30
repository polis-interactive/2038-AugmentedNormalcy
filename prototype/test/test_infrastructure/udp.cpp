//
// Created by brucegoose on 12/10/22.
//

#include <doctest.h>
#include <iostream>
#include <chrono>
using namespace std::literals;

#include <cstdlib>
#include <iostream>
#include <boost/asio.hpp>

using boost::asio::ip::udp;


#include "infrastructure/udp/context.hpp"
#include "infrastructure/udp/server.hpp"
#include "infrastructure/udp/client.hpp"

struct TestConfig:
    infrastructure::UdpContextConfig, infrastructure::UdpServerConfig, infrastructure::UdpClientPoolConfig
{
    TestConfig(int pool_size, int server_buffer_count, int server_port):
        _pool_size(pool_size),
        _server_buffer_count(server_buffer_count),
        _server_port(server_port)
    {}
    const int _pool_size;
    const int _server_buffer_count;
    const int _server_port;
    [[nodiscard]] int get_udp_pool_size() const override {
        return _pool_size;
    };
    [[nodiscard]] int get_udp_server_buffer_count() const override {
        return _server_buffer_count;
    };
    [[nodiscard]] int get_udp_server_port() const override {
        return _server_port;
    };
    [[nodiscard]] std::chrono::milliseconds get_udp_rw_timeout() const override {
        return std::chrono::milliseconds(1);
    };
};

typedef std::chrono::high_resolution_clock Clock;

TEST_CASE("Graceful context startup and teardown with some timing") {
    TestConfig conf(3, 3, 6969);
    std::chrono::time_point< std::chrono::high_resolution_clock> t1, t2, t3, t4;
    {
        t1 = Clock::now();
        infrastructure::UdpContext ctx(conf);
        t2 = Clock::now();
        ctx.Start();
        t3 = Clock::now();
        ctx.Stop();
        t4 = Clock::now();
    }
    auto d1 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
    auto d2 = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2);
    auto d3 = std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3);
    std::cout << d1.count() << ", " << d2.count() << ", " << d3.count() << std::endl;
}

TEST_CASE("Udp server startup and teardown; expect errors at creation") {
    TestConfig conf(3, 3, 6969);
    auto on_receive = [](std::shared_ptr<infrastructure::UdpServerSession> &&session) {};
    infrastructure::UdpContext ctx(conf);
    ctx.Start();
    infrastructure::UdpServer srv(conf, ctx.GetContext(), on_receive);
    srv.Start();
    // let it live for a sec
    std::this_thread::sleep_for(500ms);
    srv.Stop();
    ctx.Stop();
}

TEST_CASE("udp client startup and teardown") {
    TestConfig conf(3, 3, 6969);
    auto b_pool = udp_buffer_pool(
        18, [](){ return std::make_shared<udp_buffer>(); }
    );

    infrastructure::UdpContext ctx(conf);
    infrastructure::UdpClientPool c_pool(conf, ctx.GetContext());


    ctx.Start();

    auto client = c_pool.GetOrCreateClient("127.0.0.1", b_pool);
    // let it live for a sec
    std::this_thread::sleep_for(500ms);

    c_pool.Stop();
    ctx.Stop();
}

TEST_CASE("simple send / receive") {
    TestConfig conf(3, 12, 6969);
    auto b_pool = udp_buffer_pool(
        12, [](){ return std::make_shared<udp_buffer>(); }
    );

    std::vector<std::string> samples { "hello", "world", "bar", "baz", "foo", "bax" };
    sort(samples.begin(), samples.end());
    std::vector<std::string> output {};
    auto on_receive = [&output](std::shared_ptr<infrastructure::UdpServerSession> &&session) {
        const auto s = std::string(
            std::begin(*session->_buffer), std::begin(*session->_buffer) + session->_bytes_received
        );
        output.push_back(s);
        session.reset();
    };

    infrastructure::UdpContext ctx(conf);
    infrastructure::UdpServer srv(conf, ctx.GetContext(), on_receive);
    infrastructure::UdpClientPool c_pool(conf, ctx.GetContext());

    ctx.Start();
    srv.Start();
    auto client = c_pool.GetOrCreateClient("127.0.0.1", b_pool);

    for (auto &sample : samples) {
        auto buffer = b_pool.New();
        std::copy(sample.begin(), sample.end(), buffer->data());
        client->Send(buffer, sample.size());
    }

    // give it a few to do its thing
    std::this_thread::sleep_for(500ms);


    // check the results
    REQUIRE_EQ(samples.size(), output.size());
    // could be out of order with threads / async and what not
    sort(output.begin(), output.end());
    REQUIRE(std::equal(samples.begin(), samples.end(), output.begin()));

    // for kicks, lets ensure buffers are getting cleared
    REQUIRE_EQ(b_pool.AvailableBuffers(), 12);

    c_pool.Stop();
    srv.Stop();
    ctx.Stop();
}

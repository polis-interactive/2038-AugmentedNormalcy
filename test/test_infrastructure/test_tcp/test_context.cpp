//
// Created by brucegoose on 3/5/23.
//

#include <doctest.h>
#include <iostream>
#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;

#include "infrastructure/tcp/tcp_context.hpp"

struct TestContextConfig:
    infrastructure::TcpContextConfig
{
    explicit TestContextConfig(int pool_size):
        _pool_size(pool_size)
    {}
    const int _pool_size;
    [[nodiscard]] int get_tcp_pool_size() const override {
        return _pool_size;
    };
};


TEST_CASE("Graceful context startup and teardown with some timing") {
    TestContextConfig conf(3);
    std::chrono::time_point< std::chrono::high_resolution_clock> t1, t2, t3, t4;
    {
        t1 = Clock::now();
        infrastructure::TcpContext ctx(conf);
        t2 = Clock::now();
        ctx.Start();
        t3 = Clock::now();
        ctx.Stop();
        t4 = Clock::now();
    }
    auto d1 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
    auto d2 = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2);
    auto d3 = std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3);
    std::cout << "test_infrastructure/udp/context startup and teardown: " <<
        d1.count() << ", " << d2.count() << ", " << d3.count() << std::endl;
}

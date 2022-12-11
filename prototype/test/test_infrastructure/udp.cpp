//
// Created by brucegoose on 12/10/22.
//

#include <doctest.h>
#include <iostream>
#include <chrono>

#include "infrastructure/udp/context.hpp"

struct TestConfig: UdpContextConfig {
    TestConfig(int pool_size, int buffer_size):
        _pool_size(pool_size),
        _buffer_size(buffer_size) {}
    const int _pool_size;
    const int _buffer_size;
    [[nodiscard]] int get_pool_size() const override {
        return _pool_size;
    };
    [[nodiscard]] int get_buffer_size() const override {
        return _buffer_size;
    }
};

typedef std::chrono::high_resolution_clock Clock;

TEST_CASE("Graceful context startup and teardown with some timing") {
    TestConfig conf(3, 1024);
    std::chrono::time_point< std::chrono::system_clock> t1, t2, t3, t4;
    {
        t1 = Clock::now();
        UdpContext ctx(conf);
        t2 = Clock::now();
        std::this_thread::sleep_for(std::chrono::seconds(2));
        t3 = Clock::now();
    }
    t4 = Clock::now();
    auto d1 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
    auto d2 = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2);
    auto d3 = std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3);
    std::cout << d1.count() << ", " << d2.count() << ", " << d3.count() << std::endl;
}
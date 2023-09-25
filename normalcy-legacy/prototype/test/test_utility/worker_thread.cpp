//
// Created by bruce on 12/28/2022.
//

#include <doctest.h>
#include <vector>
#include <iostream>
#include <chrono>
using namespace std::literals;

#include "utility/worker_thread.hpp"
typedef std::chrono::high_resolution_clock Clock;

struct test_timestamp {
    test_timestamp() : inst(Clock::now()) {}
    std::chrono::time_point<Clock> inst;
    std::chrono::time_point<Clock> out_stamp;
};

TEST_CASE("We start up and stop and delete without the compiler throwing a hissy fit") {
    {
        auto callback = [](std::shared_ptr<test_timestamp> &&ts) {
            std::cout << "test_utility/worker_thread/elapsed micros: " <<
                std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - ts->inst).count() << std::endl;
        };
        auto wt = utility::WorkerThread<test_timestamp>::CreateWorkerThread(callback);
        wt->Start();
        wt->PostWork(std::make_shared<test_timestamp>());
        std::this_thread::sleep_for(500ms);
        wt->Stop();
    }
}

struct test_message {
    explicit test_message(std::string &&s) : _msg(std::move(s)) {}
    std::string _msg;
};

TEST_CASE("We can do some work!") {
    std::queue<std::shared_ptr<test_message>> output;
    auto callback = [&output](std::shared_ptr<test_message> &&tm) {
        output.push(tm);
    };
    auto wt = utility::WorkerThread<test_message>::CreateWorkerThread(callback);
    wt->Start();

    // add one, read it out
    wt->PostWork(std::make_shared<test_message>("yolo"));
    std::this_thread::sleep_for(1ms);
    REQUIRE_EQ(output.size(), 1);
    auto tm_1 = output.front();
    output.pop();
    REQUIRE_EQ(tm_1->_msg, "yolo");

    wt->PostWork(std::make_shared<test_message>("message 1"));
    wt->PostWork(std::make_shared<test_message>("message 2"));
    wt->PostWork(std::make_shared<test_message>("message 3"));
    std::this_thread::sleep_for(1ms);
    tm_1 = output.front();
    output.pop();
    REQUIRE_EQ(tm_1->_msg, "message 1");
    auto tm_2 = output.front();
    output.pop();
    REQUIRE_EQ(tm_2->_msg, "message 2");
    auto tm_3 = output.front();
    output.pop();
    REQUIRE_EQ(tm_3->_msg, "message 3");

    for (int i = 0; i < 1000; i++) {
        wt->PostWork(std::make_shared<test_message>(std::to_string(i)));
    }
    std::this_thread::sleep_for(1ms);
    REQUIRE_EQ(output.size(), 1000);
    REQUIRE_EQ(output.front()->_msg, "0");
    REQUIRE_EQ(output.back()->_msg, "999");

    wt->Stop();
}

TEST_CASE("baby stress test") {
    std::queue<std::shared_ptr<test_timestamp>> output;
    auto callback = [&output](std::shared_ptr<test_timestamp> &&ts) {
        ts->out_stamp = Clock::now();
        output.push(ts);
    };
    auto wt = utility::WorkerThread<test_timestamp>::CreateWorkerThread(callback);
    wt->Start();

    for (int i = 0; i < 100000; i++) {
        wt->PostWork(std::make_shared<test_timestamp>());
    }
    std::this_thread::sleep_for(500ms);
    REQUIRE_EQ(output.size(), 100000);

    std::cout << "test_utility/worker_thread/baby stress test: " << std::chrono::duration_cast<std::chrono::milliseconds>(
            output.back()->out_stamp - output.front()->inst
    ).count() << std::endl;

    wt->Stop();
}
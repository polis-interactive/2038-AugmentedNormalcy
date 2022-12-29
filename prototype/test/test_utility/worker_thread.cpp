//
// Created by bruce on 12/28/2022.
//

#include <doctest.h>
#include <string>
#include <utility>
#include <vector>
#include <iostream>
#include <chrono>
using namespace std::literals;

#include "utility/worker_thread.hpp"
typedef std::chrono::high_resolution_clock Clock;

struct test_timestamp {
    test_timestamp() : inst(Clock::now()) {}
    std::chrono::time_point<Clock> inst;
};

TEST_CASE("We start up and stop and delete without the compiler throwing a hissy fit") {
    {
        auto callback = [](std::shared_ptr<test_timestamp> &&ts) {
            std::cout << Clock::now() - ts->inst << std::endl;
        };
        auto wt = utility::WorkerThread<test_timestamp>::CreateWorkerThread(callback);
        wt->Start();
        wt->PostWork(std::make_shared<test_timestamp>());
        std::this_thread::sleep_for(500ms);
        wt->Stop();
    }
}

TEST_CASE("We can do some work!") {
    std::vector<test_timestamp> output;
}
//
// Created by brucegoose on 4/8/23.
//

#include <doctest.h>
#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;

#include "service/server/server_streamer.hpp"

TEST_CASE("SERVICE_SERVER-ENCODER_Setup-and-teardown") {
    service::ServerStreamerConfig conf(
        2, 69691, 3, 6, 4, 5, 4269, 5, service::ClientAssignmentStrategy::CAMERA_THEN_HEADSET,
        service::CameraSwitchingStrategy::NONE, 30
    );

    std::chrono::time_point< std::chrono::high_resolution_clock> t1, t2, t3, t4;

    {
        t1 = Clock::now();
        auto streamer = service::ServerStreamer::Create(conf);
        t2 = Clock::now();
        streamer->Start();
        t3 = Clock::now();
        streamer->Stop();
        t4 = Clock::now();
        streamer->Unset();
    }
    auto d1 = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1);
    auto d2 = std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2);
    auto d3 = std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3);
    std::cout << "test_service/test_server_encoder Setup and Teardown: microseconds: " <<
              d1.count() << ", " << d2.count() << ", " << d3.count() << std::endl;
}
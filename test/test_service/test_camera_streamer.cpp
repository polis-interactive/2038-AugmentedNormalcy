//
// Created by brucegoose on 3/22/23.
//

#include <doctest.h>

#include <utility>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
using namespace std::literals;
typedef std::chrono::high_resolution_clock Clock;


#include "service/camera_streamer.hpp"

#include "camera_streamer_server.hpp"


TEST_CASE("SERVICE_CAMERA-STREAMER_Setup-and-teardown") {
    service::CameraStreamerConfig conf(
        "127.0.0.1", 6969,
        infrastructure::CameraType::LIBCAMERA,
#if _AN_PLATFORM_ == PLATFORM_RPI
        { 1536, 864 },
#elif _AN_PLATFORM_ == PLATFORM_BROOSE_LINUX_LAPTOP
        {848, 480},
#endif
        5
    );

    std::chrono::time_point< std::chrono::high_resolution_clock> t1, t2, t3, t4;

    {
        t1 = Clock::now();
        auto streamer = service::CameraStreamer::Create(conf);
        t2 = Clock::now();
        streamer->Start();
        t3 = Clock::now();
        streamer->Stop();
        t4 = Clock::now();
        streamer->Unset();
    }
    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);
    auto d2 = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2);
    auto d3 = std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3);
    std::cout << "test_service/test_camera_streamer Setup and Teardown: milliseconds: " <<
              d1.count() << ", " << d2.count() << ", " << d3.count() << std::endl;
}

TEST_CASE("SERVICE_CAMERA-STREAMER_Holding-pattern") {
    service::CameraStreamerConfig conf(
            "127.0.0.1", 6969,
            infrastructure::CameraType::LIBCAMERA,
#if _AN_PLATFORM_ == PLATFORM_RPI
            { 1536, 864 },
#elif _AN_PLATFORM_ == PLATFORM_BROOSE_LINUX_LAPTOP
            {848, 480},
#endif
            5
    );


    {
        auto streamer = service::CameraStreamer::Create(conf);
        streamer->Start();
        std::this_thread::sleep_for(5s);
        streamer->Stop();
        streamer->Unset();
    }
}

TEST_CASE("SERVICE_CAMERA-STREAMER_Transmit-a-usable-frame") {
    service::CameraStreamerConfig streamer_conf(
            "127.0.0.1", 6969,
            infrastructure::CameraType::LIBCAMERA,
#if _AN_PLATFORM_ == PLATFORM_RPI
            { 1536, 864 },
#elif _AN_PLATFORM_ == PLATFORM_BROOSE_LINUX_LAPTOP
            {848, 480},
#endif
            5
    );

    std::filesystem::path test_dir = TEST_DIR;
    test_dir /= "test_service";

    std::filesystem::path out_frame = test_dir;
#if _AN_PLATFORM_ == PLATFORM_RPI
    out_frame /= "out.yuv";
#else
    out_frame /= "out.jpeg";
#endif

    if(std::filesystem::remove(out_frame)) {
        std::cout << "Removed output file" << std::endl;
    } else {
        std::cout << "No output file to remove" << std::endl;
    }

    std::chrono::time_point<std::chrono::high_resolution_clock> in_time, out_time;
    bool is_done = false;

    auto callback = [&out_frame, &out_time, &is_done](std::shared_ptr<ResizableBuffer> &&ptr){
        if (is_done) {
            return;
        }
        std::cout << "Writing to file" << std::endl;
        is_done = true;
        out_time = Clock::now();
        std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
        test_file_out.write((char *)ptr->GetMemory(), ptr->GetSize());
        test_file_out.flush();
        test_file_out.close();
    };

#if _AN_PLATFORM_ == PLATFORM_RPI
    std::vector<unsigned int> frame_sizes = { 1536 * 864, 1536 * 864 / 4, 1536 * 864 / 4 };
#else
    std::vector<unsigned int> frame_sizes = { 814669 };
#endif
    TestServerConfig srv_conf(3, 6969);
    auto ctx = infrastructure::TcpContext::Create(srv_conf);
    ctx->Start();
    auto manager = std::make_shared<TcpCameraServerManager>(callback);
    auto srv_manager = std::static_pointer_cast<infrastructure::TcpServerManager>(manager);
    auto srv = infrastructure::TcpServer::Create(srv_conf, ctx->GetContext(), srv_manager);
    srv->Start();

    {
        auto streamer = service::CameraStreamer::Create(streamer_conf);
        streamer->Start();
        std::this_thread::sleep_for(2s);
        REQUIRE(manager->ClientIsConnected());
        in_time = Clock::now();
        std::this_thread::sleep_for(1s);
        streamer->Stop();
        streamer->Unset();
    }
    srv->Stop();
    ctx->Stop();

    REQUIRE(std::filesystem::exists(out_frame));

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(out_time - in_time);
    std::cout << "Time to capture: " << d1.count() << std::endl;

}

TEST_CASE("SERVICE_CAMERA-STREAMER_Transmit-10-seconds") {
    service::CameraStreamerConfig streamer_conf(
        "127.0.0.1", 6969, infrastructure::CameraType::LIBCAMERA, { 1536, 864 }, 5
    );

    std::filesystem::path test_dir = TEST_DIR;
    test_dir /= "test_service";

    std::filesystem::path out_frame = test_dir;

    out_frame /= "out_150.yuv";

    if(std::filesystem::remove(out_frame)) {
        std::cout << "Removed output file" << std::endl;
    } else {
        std::cout << "No output file to remove" << std::endl;
    }

    std::chrono::time_point<std::chrono::high_resolution_clock> in_time, out_time;
    int frame_count = 0;

    auto callback = [&out_frame, &out_time, &frame_count](std::shared_ptr<ResizableBuffer> &&ptr){
        frame_count++;
        if (frame_count != 150) {
            return;
        }
        std::cout << "Writing to file" << std::endl;
        out_time = Clock::now();
        std::ofstream test_file_out(out_frame, std::ios::out | std::ios::binary);
        test_file_out.write((char *)ptr->GetMemory(), ptr->GetSize());
        test_file_out.flush();
        test_file_out.close();
    };

#if _AN_PLATFORM_ == PLATFORM_RPI
    std::vector<unsigned int> frame_sizes = { 1536 * 864, 1536 * 864 / 4, 1536 * 864 / 4 };
#else
    std::vector<unsigned int> frame_sizes = { 814669 };
#endif

    TestServerConfig srv_conf(3, 6969);
    auto ctx = infrastructure::TcpContext::Create(srv_conf);
    ctx->Start();
    auto manager = std::make_shared<TcpCameraServerManager>(callback);
    auto srv_manager = std::static_pointer_cast<infrastructure::TcpServerManager>(manager);
    auto srv = infrastructure::TcpServer::Create(srv_conf, ctx->GetContext(), srv_manager);
    srv->Start();
    {
        auto streamer = service::CameraStreamer::Create(streamer_conf);
        streamer->Start();
        in_time = Clock::now();
        std::this_thread::sleep_for(6s);
        streamer->Stop();
        streamer->Unset();
    }
    // give the last frame some time to flush
    std::this_thread::sleep_for(1ms);
    srv->Stop();
    ctx->Stop();

    auto d1 = std::chrono::duration_cast<std::chrono::milliseconds>(out_time - in_time);
    std::cout << "Time to capture: " << d1.count() << std::endl;
    std::cout << "Frames: " << frame_count << std::endl;

    REQUIRE(std::filesystem::exists(out_frame));
    REQUIRE_GT(frame_count, 149);

}

//
// Created by brucegoose on 2/19/23.
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

#include "infrastructure/camera/libcamera_camera.hpp"

struct LibcameraTestConfig : infrastructure::LibcameraConfig {
    [[nodiscard]] std::pair<int, int> get_camera_width_height() const final {
#if _AN_PLATFORM_ == PLATFORM_RPI
        return {1536, 864};
#else
        return {848, 480};
#endif
    }
    [[nodiscard]] int get_fps() const final {
        return 30;
    }
    [[nodiscard]] int get_camera_buffer_count() const final {
        return 4;
    }
};

TEST_CASE("INFRASTRUCTURE_CAMERA_LIBCAMERA-Capture_one_frame") {
    auto config = LibcameraTestConfig();

    std::filesystem::path test_dir = TEST_DIR;
    test_dir /= "test_infrastructure";
    test_dir /= "test_camera";

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

    auto callback = [&out_frame, &out_time, &is_done](std::shared_ptr<CameraBuffer> &&ptr){
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
    // scope cam so it deconstructs before REQUIRE
    {
        auto cam = infrastructure::LibcameraCamera::Create(config, callback);
        cam->Start();
        in_time = Clock::now();
        std::this_thread::sleep_for(500ms);
        cam->Stop();
    }

    REQUIRE(std::filesystem::exists(out_frame));

    auto d1 = std::chrono::duration_cast<std::chrono::microseconds>(out_time - in_time);
    std::cout << "Time to capture: " << d1.count() << std::endl;
}

// Just proving we will be able to reconfigure, start, and stop :D
#ifdef IGNORE
// useless and sometimes hangs D:
TEST_CASE("INFRASTRUCTURE_CAMERA_LIBCAMERA-Start_And_Stop_And") {
    auto config = LibcameraTestConfig();
    auto cam = infrastructure::Camera::Create(config, [](std::shared_ptr<SizedBufferPool> &&ptr){});
    for (int i = 0; i <= 10; i++) {
        cam->Start();
        std::this_thread::sleep_for(500ms);
        cam->Stop();
        std::this_thread::sleep_for(500ms);
    }
}
#endif

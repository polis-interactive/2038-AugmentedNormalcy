//
// Created by brucegoose on 4/22/23.
//


#include "service/camera_streamer.hpp"

#include <csignal>
#include <chrono>
using namespace std::literals;

std::function<void(int)> shutdown_handler;
void signal_handler(int signal) { shutdown_handler(signal); }

int main() {
    const service::CameraStreamerConfig conf(
        "192.168.1.10", 6969, infrastructure::CameraType::LIBCAMERA, { 1536, 864 }
    );
    auto service = service::CameraStreamer::Create(conf);
    service->Start();

    bool exit = false;

    shutdown_handler = [&](int signal) {
        std::cout << "Server shutdown...\n";
        exit = true;
    };

    signal(SIGINT, signal_handler);

    while(!exit){
        std::this_thread::sleep_for(1s);
    }

    service->Stop();
    std::this_thread::sleep_for(500ms);
    service->Unset();
    std::this_thread::sleep_for(500ms);
}
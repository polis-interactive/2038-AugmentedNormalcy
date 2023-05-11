//
// Created by brucegoose on 4/22/23.
//

#include "service/server_streamer.hpp"

#include <filesystem>
#include <fstream>

#include <csignal>
#include <chrono>
using namespace std::literals;

std::function<void(int)> shutdown_handler;
void signal_handler(int signal) { shutdown_handler(signal); }

int main() {

    const std::filesystem::path stop_file = "/tmp/augmented_normalcy_stopped_successfully";

    if(std::filesystem::remove(stop_file)) {
        std::cout << "Removed successful stop file" << std::endl;
    } else {
        std::cout << "No successful stop file to remove" << std::endl;
    }

    const service::ServerStreamerConfig conf(5, 6969, 4, 1990656);
    auto service = service::ServerStreamer::Create(conf);
    service->Start();

    bool exit = false;

    shutdown_handler = [&](int signal) {
        std::cout << "Server shutdown...\n";
        exit = true;
    };

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    while(!exit){
        std::this_thread::sleep_for(1s);
    }

    service->Stop();
    std::this_thread::sleep_for(500ms);
    service->Unset();
    std::this_thread::sleep_for(500ms);

    std::ofstream ofs(stop_file);
    if (!ofs) {
        std::cerr << "Failed to touch file: " << stop_file << '\n';
    } else {
        ofs.close();
    }
}
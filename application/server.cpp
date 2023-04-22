//
// Created by brucegoose on 4/22/23.
//

#include "service/server_encoder.hpp"

#include <csignal>
#include <chrono>
using namespace std::literals;

std::function<void(int)> shutdown_handler;
void signal_handler(int signal) { shutdown_handler(signal); }

int main() {
    const service::ServerEncoderConfig conf(3, 6969, 4, 6);
    auto service = service::ServerEncoder::Create(conf);
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
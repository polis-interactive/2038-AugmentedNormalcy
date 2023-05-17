//
// Created by brucegoose on 5/16/23.
//

#include "runtime.hpp"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <csignal>
#include <functional>
#include <thread>

#include <chrono>
using namespace std::literals;


namespace application {

    static const std::filesystem::path stop_file = "/tmp/augmented_normalcy_stopped_successfully";

    void RemoveSuccessFile() {
        if(std::filesystem::remove(stop_file)) {
            std::cout << "Removed successful stop file" << std::endl;
        } else {
            std::cout << "No successful stop file to remove" << std::endl;
        }
    }
    void CreateSuccessFile() {
        std::ofstream ofs(stop_file);
        if (!ofs) {
            std::cerr << "Failed to touch file: " << stop_file << '\n';
        } else {
            ofs.close();
        }
    }

    std::function<void(int)> shutdown_handler;
    void signal_handler(int signal) { shutdown_handler(signal); }

    void WaitForShutdown() {
        bool exit = false;

        shutdown_handler = [&](int signal) {
            std::cout << "shutdown...\n";
            exit = true;
        };

        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        while(!exit){
            std::this_thread::sleep_for(1s);
        }
    }
}
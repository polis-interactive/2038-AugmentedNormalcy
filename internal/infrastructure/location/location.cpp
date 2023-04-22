//
// Created by brucegoose on 4/19/23.
//

#include <SerialPort.h>
#include <iostream>

#include <dirent.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "location.hpp"

using namespace LibSerial;

namespace infrastructure {
    std::vector<std::string> EnumerateSerialPorts() {
        std::vector<std::string> serial_ports;
        const char *dir_name = "/dev";
        DIR *dir = opendir(dir_name);
        if (dir != nullptr) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string entry_name(entry->d_name);
                if (entry_name.find("ttyS") != std::string::npos || entry_name.find("ttyUSB") != std::string::npos
                    || entry_name.find("ttyACM") != std::string::npos) {
                    std::string port_name = std::string(dir_name) + "/" + entry_name;
                    int fd = open(port_name.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
                    if (fd >= 0) {
                        serial_ports.push_back(port_name);
                        close(fd);
                    }
                }
            }
            closedir(dir);
        }
        for (const auto &port : serial_ports) {
            std::cout << port << std::endl;
        }
        return std::move(serial_ports);
    }
}
//
// Created by brucegoose on 5/26/23.
//

#include <iostream>
#include <regex>

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/file.h>

#include "serial_bms.hpp"

#include "utils/clock.hpp"

namespace infrastructure {

    SerialBms::SerialBms(
        const infrastructure::BmsConfig &config, net::io_context &context, BmsMessageCallback &&post_callback
    ):
        Bms(config, context, std::move(post_callback)),
        _strand(net::make_strand(context)),
        _bms_read_timeout(config.get_bms_read_timeout()),
        _timer(_strand)
    {}

    void SerialBms::Start() {
        if (!_work_stop) {
            return;
        }
        _work_stop = false;

        auto self(shared_from_this());
        _work_thread = std::make_unique<std::thread>([this, self]() mutable { run(); });
    }

    void SerialBms::run() {
        std::cout << "SerialBms::run running" << std::endl;
        while (!_work_stop) {
            bool success = setupConnection();

            std::cout << "SerialBms::run successfully connected" << std::endl;

            if (success) {
                readAndReport();
            }

            if (_port_fd >= 0) {
                close(_port_fd);
                _port_fd = -1;
            }
        }
    }

    bool SerialBms::setupConnection() {
        std::cout << "SerialBms::setupConnection Opening file" << std::endl;
        _port_fd = open("/dev/ttyAMA0", O_RDWR|O_NOCTTY);
        if (_port_fd < 0) {
            std::cout << "SerialBms::setupConnection failed to open /dev/ttyAMA0" << std::endl;
            return false;
        }

        if(flock(_port_fd, LOCK_EX | LOCK_NB) == -1) {
            std::cout << "SerialBms::setupConnection failed to lock serial connection /dev/ttyAMA0" << std::endl;
            return false;
        }

        struct termios tty;
        std::cout << "SerialBms::setupConnection making connection object" << std::endl;
        if(tcgetattr(_port_fd, &tty) != 0) {
            std::cout << "SerialBms::setupConnection failed to get tty setup" << std::endl;
            return false;
        }

        // 8n1
        tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
        tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
        tty.c_cflag &= ~CSIZE; // Clear all bits that set the data size
        tty.c_cflag |= CS8; // 8 bits per byte (most common)


        // raw, no echo
        tty.c_iflag &= ~(IGNBRK | IGNCR | INLCR | ICRNL | IUCLC |
                         IXANY | IXON | IXOFF | INPCK | ISTRIP);
        tty.c_iflag |= (BRKINT | IGNPAR);
        tty.c_oflag &= ~OPOST;
        tty.c_lflag &= ~(XCASE|ECHONL|NOFLSH);
        tty.c_lflag &= ~(ICANON | ISIG | ECHO);
        tty.c_cflag |= CREAD;
        tty.c_cc[VTIME] = 5;
        tty.c_cc[VMIN] = 1;

        tty.c_iflag &= ~(IXOFF | IXON); // disable software flow control
        tty.c_cflag |= CRTSCTS; // enable hardware flow controll


        std::cout << "SerialBms::setupConnection setting speed" << std::endl;

        bool success = cfsetispeed(&tty, B9600);
        if (success != 0) {
            std::cout << "SerialBms::setupConnection failed to set output speed" << std::endl;
            return false;
        }

        success = cfsetispeed(&tty, B9600);
        if (success != 0) {
            std::cout << "SerialBms::setupConnection failed to set input speed" << std::endl;
            return false;
        }

        std::cout << "SerialBms::setupConnection setting terminal attrs" << std::endl;
        success = tcsetattr(_port_fd, TCSANOW, &tty);
        if (success != 0) {
            std::cout << "SerialBms::setupConnection failed to set serial parameters" << std::endl;
            return false;
        }

        return true;
    }

    void SerialBms::readAndReport() {

        std::cout << "SerialBms::readAndReport running" << std::endl;

        static char breaker = '\n';

        while (!_work_stop) {

            int total_bytes_read = 0;
            std::memset(_bms_read_buffer.data(), 0, _bms_read_buffer.size());

            while (!_work_stop) {

                fd_set readfs; /* file descriptor set */
                FD_ZERO(&readfs);
                FD_SET(_port_fd, &readfs);

                struct timeval Timeout;
                Timeout.tv_usec = 0;  /* microseconds */
                Timeout.tv_sec = 1;  /* seconds */
                int ready_descriptors = select(_port_fd+1, &readfs, NULL, NULL, &Timeout);
                if(ready_descriptors < 0) {
                    std::cout << "SerialBms::readAndReport select failed; leaving" << std::endl;
                    return;
                } else if(ready_descriptors == 0) {
                    std::cout << "SerialBms::readAndReport not ready" << std::endl;
                    continue;
                }

                auto bytes_read = read(
                        _port_fd, _bms_read_buffer.data() + total_bytes_read,
                        _bms_read_buffer.size() - total_bytes_read - 1
                );
                if (bytes_read < 0) {
                    std::cout << "SerialBms::readAndReport read failed; leaving" << std::endl;
                    return;
                } else if (bytes_read == 0) {
                    std::cout << "SerialBms::readAndReport read EOF while trying to read: " <<
                        _bms_read_buffer.size() - total_bytes_read - 1 << "leaving" << std::endl;
                    return;
                }
                total_bytes_read += bytes_read;
                if (total_bytes_read < (_bms_read_buffer.size() - 10)) {
                    continue;
                }

                std::string response(_bms_read_buffer.begin(), _bms_read_buffer.begin() + total_bytes_read);
                std::cout << response << std::endl;
                auto [success, bms_message] = tryParseResponse(response);

                if (!success) {
                    std::cout << "SerialBms::readAndReport parse string; leaving" << std::endl;
                    return;
                } else {
                    _post_callback(bms_message);
                    break;
                }

            }

        }

        std::cout << "SerialBms::readAndReport stopping" << std::endl;
    }

    std::pair<bool, BmsMessage> SerialBms::tryParseResponse(const std::string &input) {
        const static std::string version_clause = "SmartUPS V3.2P";
        const static std::regex vin_pattern(R"(Vin\s*(NG|GOOD))");
        const static std::regex batcap_pattern(R"(BATCAP\s*(100|[1-9]?[0-9]))");

        const static std::string plugged_in_string = "GOOD";

        BmsMessage msg{};

        // make sure we have a good packet
        std::smatch result;
        if (std::count(input.begin(), input.end(), '$') < 2) {
            std::cout << "SerialBms::tryParseResponse failed to parse the string" << std::endl;
            return { false, msg };
        }

        // check that it's reporting the correct version
        if (input.find(version_clause) == std::string::npos) {
            std::cout << "SerialBms::tryParseResponse couldn't find version clause in payload" << std::endl;
            return { false, msg };
        }

        // check if its plugged in or not
        bool ret = std::regex_search(input, result, vin_pattern);
        if (!ret) {
            std::cout << "SerialBms::tryParseResponse failed to find vin in payload" << std::endl;
            return { false, msg };
        }
        msg.bms_is_plugged_in = result[1] == plugged_in_string;

        // check the battery level
        ret = std::regex_search(input, result, batcap_pattern);
        if (!ret) {
            std::cout << "SerialBms::tryParseResponse failed to find batcap in payload" << std::endl;
            return { false, msg };
        }
        msg.battery_level = std::stoi(result[1]);
        msg.bms_wants_shutdown = msg.battery_level <= _bms_shutdown_threshold;

        return { true, msg };
    }

    void SerialBms::Stop() {
        if (_work_stop) {
            return;
        }

        if (_work_thread) {
            if (_work_thread->joinable()) {
                _work_stop = true;
                _work_thread->join();
            }
            _work_thread.reset();
        }

        // just in case we skipped above
        _work_stop = true;
    }

    SerialBms::~SerialBms() {
        Stop();
    }
}
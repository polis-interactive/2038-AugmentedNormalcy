//
// Created by brucegoose on 5/26/23.
//

#include <iostream>
#include <regex>

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

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
        bool has_run = false;
        while (!_work_stop) {
            if (has_run) {
                std::this_thread::sleep_for(1s);
            }
            has_run = true;

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

    void SerialBms::startConnection(const bool is_initial_connection) {
        auto self(shared_from_this());
        net::post(_strand, [this, self, is_initial_connection]() mutable {
            doStartConnection(is_initial_connection);
        });
    }

    bool SerialBms::setupConnection() {
        std::cout << "SerialBms::setupConnection Opening file" << std::endl;
        _port_fd = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY);
        if (_port_fd < 0) {
            std::cout << "SerialBms::setupConnection failed to open /dev/ttyAMA0" << std::endl;
            return false;
        }

        struct termios tty;
        std::cout << "SerialBms::setupConnection making connection object" << std::endl;
        if(tcgetattr(_port_fd, &tty) != 0) {
            std::cout << "SerialBms::setupConnection failed to get tty setup" << std::endl;
            return false;
        }

        tty.c_cflag &= ~PARENB; // Clear parity bit, disabling parity (most common)
        tty.c_cflag &= ~CSTOPB; // Clear stop field, only one stop bit used in communication (most common)
        tty.c_cflag &= ~CSIZE; // Clear all bits that set the data size
        tty.c_cflag |= CS8; // 8 bits per byte (most common)
        tty.c_cflag &= ~CRTSCTS; // Disable RTS/CTS hardware flow control (most common)
        tty.c_cflag |= CREAD | CLOCAL; // Turn on READ & ignore ctrl lines (CLOCAL = 1)

        tty.c_lflag &= ~ICANON;
        tty.c_lflag &= ~ECHO; // Disable echo
        tty.c_lflag &= ~ECHOE; // Disable erasure
        tty.c_lflag &= ~ECHONL; // Disable new-line echo
        tty.c_lflag &= ~ISIG; // Disable interpretation of INTR, QUIT and SUSP
        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
        tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes

        tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
        tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

        tty.c_cc[VTIME] = 10;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
        tty.c_cc[VMIN] = 0;

        std::cout << "SerialBms::setupConnection setting output speed" << std::endl;
        bool success = cfsetospeed(&tty, B9600); // set output speed
        if (success != 0) {
            std::cout << "SerialBms::setupConnection failed to set output speed" << std::endl;
            return false;
        }

        std::cout << "SerialBms::setupConnection setting input speed" << std::endl;
        success = cfsetispeed(&tty, B9600); // set input speed
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
        while (!_work_stop) {
            std::cout << "SerialBms::readAndReport running" << std::endl;
            int bytes_available = 0;
            if (ioctl(_port_fd, FIONREAD, &bytes_available) == -1) {
                std::cout << "SerialBms::readAndReport failed to query the port" << std::endl;
                return;
            }
            if (bytes_available > _bms_read_buffer.size()) {

                std::cout << "SerialBms::readAndReport trying to read bytes" << std::endl;
                if (!doReadBytes()) {
                    return;
                }

                std::cout << "SerialBms::readAndReport readbytes!" << std::endl;

                std::string response(std::begin(_bms_read_buffer), std::end(_bms_read_buffer));
                auto [success, bms_message] = tryParseResponse(response);

                if (!success) {
                    std::cout << "SerialBms::readAndReport: failed to parse this: " << response << std::endl;
                    return;
                } else {
                    std::cout << "SerialBms::readAndReport: Parsed successfully!" << std::endl;
                    _post_callback(bms_message);
                }

            } else {
                std::cout << "SerialBms::readAndReport not enough bytes to read: " << bytes_available << std::endl;
            }
            std::this_thread::sleep_for(1s);
        }
    }

    bool SerialBms::doReadBytes() {
        std::cout << "SerialBms::doReadBytes reading bytes" << std::endl;
        std::size_t bytes_read = 0;
        while(true) {
            auto just_read = read(_port_fd, _bms_read_buffer.data() + bytes_read, sizeof(_bms_read_buffer) - bytes_read);
            if (just_read < 0) {
                std::cout << "SerialBms::doReadBytes: failed to read" << std::endl;
                return false;
            }
            bytes_read += bytes_read;
            if (bytes_read < _bms_read_buffer.size()) {
                std::cout << "SerialBms::doReadBytes: haven't read enough bytes: only " << bytes_read << std::endl;
                std::this_thread::sleep_for(250ms);
            } else {
                return true;
            }
        }
    }

    void SerialBms::doStartConnection(const bool is_initial_connection) {
        if (!is_initial_connection) {
            if (_work_stop) return;
            std::this_thread::sleep_for(1s);
        }
        if (_work_stop) return;
        if (_port == nullptr || !_port->is_open()) {
            _port = std::make_shared<serial_port>(_strand);

            error_code ec;
            _port->open("/dev/ttyAMA0", ec);
            if (ec) {
                std::cout << "SerialBms::doStartConnection unable to open serial port" << std::endl;
                startConnection(true);
            }

            _port->set_option(serial_port::baud_rate(9600));
            _port->set_option(serial_port::character_size(8));
            _port->set_option(serial_port::flow_control(serial_port::flow_control::none));
            _port->set_option(serial_port::parity(serial_port::parity::none));
            _port->set_option(serial_port::stop_bits(serial_port::stop_bits::one));
        }
        waitReadPort();
    }

    void SerialBms::waitReadPort() {
        if (_work_stop) return;

        int fd = _port->native_handle();
        int bytes_available;
        ioctl(fd, FIONREAD, &bytes_available);
        if (bytes_available >= _bms_read_buffer.size()) {
            _bms_read_buffer.fill({});
            readPort(0);
            return;
        }

        _timer.expires_from_now(boost::posix_time::milliseconds(100));
        auto self(shared_from_this());
        _timer.async_wait([this, self](const error_code& ec) {
            if (!ec) {
                waitReadPort();
            } else {
                std::cout << "SerialBms::waitReadPort: error waiting for timer?" << std::endl;
                disconnect();
            }
        });

    }



    void SerialBms::readPort(std::size_t last_bytes) {
        // might add a read timer here
        auto self(shared_from_this());
        net::async_read(
            *_port,
            net::buffer(_bms_read_buffer.data() + last_bytes, _bms_read_buffer.size() - last_bytes),
            [this, self, last_bytes] (error_code ec, std::size_t bytes_read) mutable {
                if (_work_stop) return;
                auto total_bytes = last_bytes + bytes_read;
                if (!ec) {
                    if (total_bytes == _bms_read_buffer.size()) {
                        parseAndSendResponse();
                    } else {
                        std::this_thread::sleep_for(500ms);
                        std::cout << "Only read n bytes:" << total_bytes << std::endl;
                        readPort(total_bytes);
                    }
                } else {
                    std::cout << "SerialBms::readPort: error reading data: " << ec << "; reconnecting" << std::endl;
                    disconnect();
                }
            }
        );
    }

    void SerialBms::parseAndSendResponse() {
        std::string response(std::begin(_bms_read_buffer), std::end(_bms_read_buffer));
        auto [success, bms_message] = tryParseResponse(response);
        if (!success) {
            std::cout << "failed to parse this: " << response << std::endl;
            disconnect();
        } else {
            _post_callback(bms_message);
            waitReadPort();
        }
    }

    void SerialBms::disconnect() {
        if (_port != nullptr) {
            if (_port->is_open()) {
                _port->close();
            }
            _port = nullptr;
        }
        if (!_work_stop) {
            startConnection(false);
        }
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
//
// Created by brucegoose on 5/26/23.
//

#include <iostream>
#include <regex>

#include "serial_bms.hpp"

#include "utils/clock.hpp"

namespace infrastructure {

    SerialBms::SerialBms(
        const infrastructure::BmsConfig &config, net::io_context &context, BmsMessageCallback &&post_callback
    ):
        Bms(config, context, std::move(post_callback)),
        _strand(net::make_strand(context)),
        _bms_read_timeout(config.get_bms_read_timeout())
    {}

    void SerialBms::Start() {
        if (!_work_stop) {
            return;
        }
        _work_stop = false;
        startConnection(false);
    }

    void SerialBms::startConnection(const bool is_initial_connection) {
        auto self(shared_from_this());
        net::post(_strand, [this, self, is_initial_connection]() mutable {
            doStartConnection(is_initial_connection);
        });
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
            _port->set_option(serial_port::flow_control(serial_port::flow_control::hardware));
            _port->set_option(serial_port::parity(serial_port::parity::none));
            _port->set_option(serial_port::stop_bits(serial_port::stop_bits::one));
        }

        _bms_read_buffer.fill({});
        readPort(0);
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
                        std::cout << "uhoh, need to read more" << std::endl;
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
            _bms_read_buffer.fill({});
            readPort(0);
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
        const static std::regex wrapping_pattern(R"(\$((.|\n|\r)*?) \$)");
        const static std::string version_clause = "SmartUPS V3.2P";
        const static std::regex vin_pattern(R"(Vin\s*(NG|GOOD))");
        const static std::regex batcap_pattern(R"(BATCAP\s*(100|[1-9]?[0-9]))");

        const static std::string plugged_in_string = "GOOD";

        BmsMessage msg{};

        // make sure we have a good packet
        std::smatch result;
        bool ret = std::regex_search(input, result, wrapping_pattern);
        if (!ret) {
            std::cout << "SerialBms::tryParseResponse failed to parse the string" << std::endl;
            return { false, msg };
        }

        // check that it's reporting the correct version
        if (input.find(version_clause) == std::string::npos) {
            std::cout << "SerialBms::tryParseResponse couldn't find version clause in payload" << std::endl;
            return { false, msg };
        }

        // check if its plugged in or not
        ret = std::regex_search(input, result, vin_pattern);
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
        _work_stop = true;
        std::promise<void> done_promise;
        auto done_future = done_promise.get_future();
        auto self(shared_from_this());
        net::post(
            _strand,
            [this, self, p = std::move(done_promise)]() mutable {
                disconnect();
                p.set_value();
            }
        );
        done_future.wait();
    }

    SerialBms::~SerialBms() {
        Stop();
    }
}
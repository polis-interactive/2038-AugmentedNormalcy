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
        auto self(shared_from_this());
        _work_thread = std::make_unique<std::thread>([this, self]() mutable { run(); });
    }

    void SerialBms::run() {
        bool has_run = false;
        while (!_work_stop) {
            if (has_run) {
                // might want to make this timeout cleaner; waiting for 5s can be a bummer
                std::this_thread::sleep_for(5s);
            }
            has_run = true;
            try {
                serial_port port(_strand);
                error_code ec;
                port.open("/dev/ttyAMA0", ec);
                if (ec) {
                    throw std::runtime_error("Unable to open serial port");
                }

                port.set_option(serial_port::baud_rate(9600));
                port.set_option(serial_port::character_size(8));
                port.set_option(serial_port::flow_control(serial_port::flow_control::none));
                port.set_option(serial_port::parity(serial_port::parity::none));
                port.set_option(serial_port::stop_bits(serial_port::stop_bits::one));



                auto last_read = Clock::now();
                std::array<char, 100> buffer;

                while (!_work_stop) {

                    // poll every second in case we need to stop
                    const auto now = Clock::now();
                    const auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_read);
                    if (duration.count() < _bms_polling_timeout) {
                        std::this_thread::sleep_for(1s);
                        continue;
                    }
                    last_read = now;

                    asyncReadWithTimeout(port, buffer);

                    std::string response(std::begin(buffer), std::end(buffer));
                    // TODO: try harder to parse the message; fail three readings in a row before giving up
                    const auto message = tryParseResponse(response);
                    _post_callback(message);
                }

            } catch (std::exception &e) {
                std::cerr << "SerialBms::run Error: " << e.what() << std::endl;
            }
        }
    }

    template<std::size_t size>
    void SerialBms::asyncReadWithTimeout(serial_port &port, std::array<char, size> &buffer) {

        std::promise<bool> done_promise;
        auto done_future = done_promise.get_future();

        auto self(shared_from_this());
        net::async_read(port, net::buffer(buffer, size),
            [this, self, p = std::move(done_promise)](const error_code& ec, std::size_t bytes_written) mutable {
                if (!ec) {
                    p.set_value(true);
                } else {
                    p.set_value(false);
                }
            }
        );
        auto status = done_future.wait_for(std::chrono::seconds(_bms_read_timeout));
        if (status == std::future_status::ready) {
            if (done_future.get()) {
                return;
            } else {
                throw std::runtime_error("asyncReadWithTimeout had an error reading");
            }
        } else {
            throw std::runtime_error("asyncReadWithTimeout did not return in time");
        }
    }

    BmsMessage SerialBms::tryParseResponse(const std::string &input) {
        const static std::regex wrapping_pattern(R"(\$ (.*?) \$)");
        const static std::string version_clause = "SmartUPS V3.2P,";
        const static std::regex vin_pattern(R"(,Vin (NG|GOOD),)");
        const static std::regex batcap_pattern(R"(BATCAP (100|[1-9]?[0-9]),)");

        const static std::string plugged_in_string = "GOOD";

        BmsMessage msg{};

        // make sure we have a good packet
        std::smatch result;
        bool ret = std::regex_search(input, result, wrapping_pattern);
        if (!ret) {
            throw std::runtime_error("tryParseResponse failed to parse the string" + input);
        }
        std::string inner_payload = result[1];

        // check that it's reporting the correct version
        if (inner_payload.find(version_clause) == std::string::npos) {
            throw std::runtime_error("tryParseResponse couldn't find version clause in payload " + inner_payload);
        }

        // check if its plugged in or not
        ret = std::regex_search(inner_payload, result, vin_pattern);
        if (!ret) {
            throw std::runtime_error("tryParseResponse failed to find vin in payload " + inner_payload);
        }
        msg.bms_is_plugged_in = result[1] == plugged_in_string;

        // check the battery level
        ret = std::regex_search(inner_payload, result, batcap_pattern);
        if (!ret) {
            throw std::runtime_error("tryParseResponse failed to find batcap in payload " + inner_payload);
        }
        msg.battery_level = std::stoi(result[1]);
        msg.bms_wants_shutdown = msg.battery_level <= _bms_shutdown_threshold;

        return msg;
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
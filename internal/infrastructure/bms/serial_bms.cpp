//
// Created by brucegoose on 5/26/23.
//

#include <iostream>
#include <regex>

#include <CppLinuxSerial/SerialPort.hpp>
using namespace mn::CppLinuxSerial;


#include "serial_bms.hpp"

#include "utils/clock.hpp"


namespace infrastructure {

    SerialBms::SerialBms(
        const infrastructure::BmsConfig &config, domain::BmsMessageCallback &&post_callback
    ):
        Bms(config, std::move(post_callback)),
        _bms_read_timeout(config.get_bms_read_timeout())
    {
        _bms_read_buffer.reserve(250);
    }

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
            std::shared_ptr<SerialPort> serial_port = nullptr;
            try {
                serial_port = std::make_shared<SerialPort>(
                    "/dev/ttyAMA0", BaudRate::B_9600, NumDataBits::EIGHT, Parity::NONE, NumStopBits::ONE,
                    HardwareFlowControl::ON, SoftwareFlowControl::OFF
                );
                serial_port->SetTimeout(_bms_read_timeout);
                serial_port->Open();
                readAndReport(serial_port);
            } catch (const std::exception& ex) {
                std::cout << "SerialBms::run reported error: " << ex.what() << std::endl;
            } catch (...) {
                std::cout << "SerialBms::run reported unknown error" << std::endl;
            }
            if (serial_port != nullptr) {
                serial_port->Close();
                serial_port.reset();

            }
        }
    }

    void SerialBms::readAndReport(std::shared_ptr<SerialPort> &serial_port) {

        std::cout << "SerialBms::readAndReport running" << std::endl;

        while (!_work_stop) {

            if (serial_port->Available() < 100) {
                std::this_thread::sleep_for(250ms);
                continue;
            }
            _bms_read_buffer.clear();
            serial_port->ReadBinary(_bms_read_buffer);

            std::string response(_bms_read_buffer.begin(), _bms_read_buffer.end());
            auto [success, bms_message] = tryParseResponse(response);

            if (!success) {
                std::cout << "SerialBms::readAndReport parse string; leaving" << std::endl;
                return;
            } else {
                _post_callback(bms_message);
            }

        }

        std::cout << "SerialBms::readAndReport stopping" << std::endl;
    }

    std::pair<bool, domain::BmsMessage> SerialBms::tryParseResponse(const std::string &input) {
        const static std::string version_clause = "SmartUPS V3.2P";
        const static std::regex vin_pattern(R"(Vin\s*(NG|GOOD))");
        const static std::regex batcap_pattern(R"(BATCAP\s*(100|[1-9]?[0-9]))");

        const static std::string plugged_in_string = "GOOD";

        domain::BmsMessage msg{};

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
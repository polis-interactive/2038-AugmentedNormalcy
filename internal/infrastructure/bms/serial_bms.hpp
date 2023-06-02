//
// Created by brucegoose on 5/26/23.
//

#ifndef INFRASTRUCTURE_BMS_SERIAL_BMS_HPP
#define INFRASTRUCTURE_BMS_SERIAL_BMS_HPP

#include <thread>
#include <atomic>
#include <vector>

#include "bms.hpp"


namespace mn::CppLinuxSerial {
    class SerialPort;
}

namespace infrastructure {
    class SerialBms: public Bms, public std::enable_shared_from_this<SerialBms> {
    public:
        SerialBms(const BmsConfig &config, domain::BmsMessageCallback &&post_callback);
        void Start() override;
        void Stop() override;
        ~SerialBms();
    private:
        void run();
        bool setupConnection();
        void readAndReport(std::shared_ptr<mn::CppLinuxSerial::SerialPort> &serial_port);
        std::pair<bool, domain::BmsMessage> tryParseResponse(const std::string &input);

        std::atomic<bool> _work_stop = { true };
        std::unique_ptr<std::thread> _work_thread;

        const int _bms_read_timeout;
        std::vector<uint8_t> _bms_read_buffer;
    };
}

#endif //INFRASTRUCTURE_BMS_SERIAL_BMS_HPP

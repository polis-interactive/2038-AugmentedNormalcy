//
// Created by brucegoose on 5/26/23.
//

#ifndef INFRASTRUCTURE_BMS_SERIAL_BMS_HPP
#define INFRASTRUCTURE_BMS_SERIAL_BMS_HPP

#include <thread>
#include <atomic>

#include "bms.hpp"

using net::serial_port;

namespace infrastructure {
    class SerialBms: public Bms, public std::enable_shared_from_this<SerialBms> {
    public:
        SerialBms(const BmsConfig &config, net::io_context &context, BmsMessageCallback &&post_callback);
        void Start() override;
        void Stop() override;
        ~SerialBms();
    private:
        void run();
        bool setupConnection();
        void readAndReport();
        std::pair<bool, BmsMessage> tryParseResponse(const std::string &input);

        std::atomic<bool> _work_stop = { true };
        std::unique_ptr<std::thread> _work_thread;
        int _port_fd = -1;


        net::strand<net::io_context::executor_type> _strand;

        std::shared_ptr<serial_port> _port;

        const int _bms_read_timeout;
        std::array<char, 100> _bms_read_buffer;
        net::deadline_timer _timer;
    };
}

#endif //INFRASTRUCTURE_BMS_SERIAL_BMS_HPP

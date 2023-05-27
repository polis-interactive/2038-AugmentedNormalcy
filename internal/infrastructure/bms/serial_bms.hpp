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
        void Start();
        void Stop();
        ~SerialBms();
    private:
        void run();
        BmsMessage tryParseResponse(const std::string &input);
        void startTimer();

        template<std::size_t size>
        void asyncReadWithTimeout(serial_port &port, std::array<char, size> &buffer);

        std::unique_ptr<std::thread> _work_thread;
        std::atomic<bool> _work_stop = { true };
        net::strand<net::io_context::executor_type> _strand;

        const int _bms_read_timeout;
    };
}

#endif //INFRASTRUCTURE_BMS_SERIAL_BMS_HPP

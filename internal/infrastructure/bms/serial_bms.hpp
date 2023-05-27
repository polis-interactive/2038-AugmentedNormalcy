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
        void startConnection(const bool is_initial_connection);
        void doStartConnection(const bool is_initial_connection);
        void readPort(std::size_t last_bytes);
        void parseAndSendResponse();
        std::pair<bool, BmsMessage> tryParseResponse(const std::string &input);
        void disconnect();

        std::atomic<bool> _work_stop = { true };
        net::strand<net::io_context::executor_type> _strand;

        std::shared_ptr<serial_port> _port;

        const int _bms_read_timeout;
        std::array<char, 100> _bms_read_buffer;
    };
}

#endif //INFRASTRUCTURE_BMS_SERIAL_BMS_HPP

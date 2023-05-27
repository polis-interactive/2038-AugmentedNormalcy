//
// Created by brucegoose on 5/26/23.
//

#ifndef UTILS_MESSAGES_HPP
#define UTILS_MESSAGES_HPP

#include <stdint.h>

struct BmsMessage {
    int battery_level;
    bool bms_is_plugged_in;
    bool bms_wants_shutdown;
};

typedef std::function<void(const BmsMessage)> BmsMessageCallback;

#endif //UTILS_MESSAGES_HPP

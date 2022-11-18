//
// Created by bruce on 12/31/2022.
//

#ifndef INFRASTRUCTURE_COMMON_HPP
#define INFRASTRUCTURE_COMMON_HPP

#include <array>
#include <tuple>
#include "utility/buffer_pool.hpp"

#define MAX_FRAME_LENGTH 64000
#define MAX_REPLY_LENGTH 1024

using payload_buffer = std::array<uint8_t, MAX_FRAME_LENGTH>;
using payload_buffer_pool = utility::BufferPool<payload_buffer>;

using payload_tuple = std::tuple<std::shared_ptr<payload_buffer>&, std::size_t&>;
struct QueuedPayloadReceive {
    [[nodiscard]]  virtual payload_tuple GetPayload() = 0;
};
struct PayloadSend {
    [[nodiscard]] virtual std::shared_ptr<payload_buffer> GetBuffer() = 0;
    virtual void Send(std::shared_ptr<payload_buffer> &&buffer, std::size_t buffer_size) = 0;
    // right now, we don't care about responding...
};

using reply_buffer = std::array<uint8_t, MAX_REPLY_LENGTH>;
using reply_buffer_pool = utility::BufferPool<reply_buffer>;


const auto rolling_less_than = [](const uint16_t &a, const uint16_t &b) {
    // a == b, not less than
    // a < b if b - a is < 2^(16 - 1) (less than a big roll-over)
    // a > b if a - b is < 2^(16 - 1) (a is bigger but there is a huge roll-over)
    return a != b and (
        (a < b and (b - a) < std::pow(2, 16 - 1)) or
        (a > b and (a - b) > std::pow(2,16 - 1))
    );
};

#ifdef __GNUC__
#define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif


#endif //INFRASTRUCTURE_COMMON_HPP

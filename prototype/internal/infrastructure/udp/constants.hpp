//
// Created by bruce on 12/29/2022.
//

#ifndef AUGMENTEDNORMALCY_PROTOTYPE_CONSTANTS_HPP
#define AUGMENTEDNORMALCY_PROTOTYPE_CONSTANTS_HPP

#include <array>
#include "utility/buffer_pool.hpp"

#define MAX_PACKET_LENGTH 64000
#define MAX_REPLY_LENGTH 1024

using udp_buffer = std::array<uint8_t, MAX_PACKET_LENGTH>;
using udp_buffer_pool = utility::BufferPool<udp_buffer>;

using udp_reply_buffer = std::array<uint8_t, MAX_REPLY_LENGTH>;
using udp_reply_buffer_pool = utility::BufferPool<udp_reply_buffer>;

#endif //AUGMENTEDNORMALCY_PROTOTYPE_CONSTANTS_HPP

//
// Created by bruce on 12/31/2022.
//

#ifndef AUGMENTEDNORMALCY_PROTOTYPE_PACKET_HPP
#define AUGMENTEDNORMALCY_PROTOTYPE_PACKET_HPP

#include <cstdint>
#include <algorithm>

#include "infrastructure/common.hpp"

PACK(struct PacketHeader {
    static std::unique_ptr<PacketHeader> TryParseFrame(
        const std::shared_ptr<payload_buffer> &buffer, const std::size_t &frame_size
    ) {
        // invalid packet
        if (frame_size <= sizeof(PacketHeader)) {
            return nullptr;
        }
        // parse the header (-8 for the ptr)
        auto header = std::make_unique<PacketHeader>();
        memcpy(header.get(), buffer->data(), sizeof(PacketHeader) - 8);
        // make sure data length is valid; should be header minus the start ptr
        if (header->data_length != frame_size - (sizeof(PacketHeader) - 8)) {
            return nullptr;
        }
        header->data_start = buffer->data() + (sizeof(PacketHeader) - 8);
        return header;
    }
    uint8_t id;
    uint16_t sequence_number;
    uint16_t session_number;
    uint16_t timestamp;
    uint16_t data_length;
    uint8_t *data_start;
});

#endif //AUGMENTEDNORMALCY_PROTOTYPE_PACKET_HPP

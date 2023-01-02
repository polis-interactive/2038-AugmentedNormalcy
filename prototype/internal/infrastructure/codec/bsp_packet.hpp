//
// Created by bruce on 12/31/2022.
//

#ifndef AUGMENTEDNORMALCY_PROTOTYPE_BSP_PACKET_HPP
#define AUGMENTEDNORMALCY_PROTOTYPE_BSP_PACKET_HPP

#include <cstdint>
#include <algorithm>

#include "infrastructure/common.hpp"

// Broose Streaming Protocol Packet :D

namespace infrastructure {
    PACK(struct BspPacket {
         [[nodiscard]] std::size_t PackFrame(const std::vector<uint8_t> &payload, std::shared_ptr<payload_buffer> &buffer) {
             data_length = payload.size();
             memcpy(buffer->data(), this, sizeof(BspPacket) - 8);
             memcpy(buffer->data() + sizeof(BspPacket) - 8, payload.data(), payload.size());
             return sizeof(BspPacket) - 8 + payload.size();
         }
         [[nodiscard]] static std::unique_ptr<BspPacket> TryParseFrame(
            const std::shared_ptr<payload_buffer> &buffer, const std::size_t &frame_size
         ) {
             // invalid packet
             if (frame_size <= sizeof(BspPacket)) {
                 return nullptr;
             }
             // parse the header (-8 for the ptr)
             auto header = std::make_unique<BspPacket>();
             memcpy(header.get(), buffer->data(), sizeof(BspPacket) - 8);
             // make sure data length is valid; should be header minus the start ptr
             if (header->data_length != frame_size - (sizeof(BspPacket) - 8)) {
                 return nullptr;
             }
             header->data_start = buffer->data() + (sizeof(BspPacket) - 8);
             return header;
         }
         uint8_t id;
         uint16_t sequence_number;
         uint16_t session_number;
         uint16_t timestamp;
         uint16_t data_length;
         uint8_t *data_start;
    });
}


#endif //AUGMENTEDNORMALCY_PROTOTYPE_BSP_PACKET_HPP

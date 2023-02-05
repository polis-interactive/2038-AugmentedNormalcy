//
// Created by bruce on 12/31/2022.
//

#ifndef AUGMENTEDNORMALCY_PROTOTYPE_BSP_PACKET_HPP
#define AUGMENTEDNORMALCY_PROTOTYPE_BSP_PACKET_HPP

#include <cstdint>
#include <algorithm>
#include <cstring>

#include "infrastructure/common.hpp"

// Broose Streaming Protocol Packet :D

namespace Codec {
    PACK(struct BspPacket {
        [[nodiscard]] std::size_t PackFrame(const std::vector<uint8_t> &payload, std::shared_ptr<void> &buffer) {
            data_length = payload.size();
            memcpy((uint8_t *) buffer.get(), this, HeaderSize());
            memcpy((uint8_t *) buffer.get() + HeaderSize(), payload.data(), payload.size());
            return sizeof(BspPacket) - 8 + payload.size();
        }
        [[nodiscard]] static std::size_t HeaderSize() {
            // -8 for the ptr to data
            return sizeof(BspPacket) - sizeof(uint8_t *);
        }
        [[nodiscard]] static std::unique_ptr<BspPacket> TryParseFrame(
            const void *buffer, const std::size_t &frame_size
        ) {
             if (frame_size <= sizeof(BspPacket)) {
                 return nullptr;
             }
             auto header = std::make_unique<BspPacket>();
             memcpy(header.get(), buffer, HeaderSize());
             // make sure data length is valid; should be header minus the start ptr
             if (header->data_length != frame_size - (HeaderSize())) {
                 return nullptr;
             }
             header->data_start = (uint8_t *)buffer + HeaderSize();
             return header;
        }
        void Pack(std::shared_ptr<void> &buffer, const std::size_t &body_size) {
            data_length = body_size;
            memcpy((uint8_t *)buffer.get(), this, HeaderSize());
        }
        [[nodiscard]] std::size_t PacketSize() const {
            return data_length + HeaderSize();
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

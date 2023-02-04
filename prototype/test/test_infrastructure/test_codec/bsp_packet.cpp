//
// Created by bruce on 1/1/2023.
//

#include <doctest.h>
#include <string>
#include <sstream>
#include <iostream>

#include "infrastructure/codec/bsp_packet.hpp"

TEST_CASE("Do I know what I'm doing, grabbing buffers and what not") {
    std::string payload_in = "Hello, world!";
    std::vector<uint8_t> payload(payload_in.begin(), payload_in.end());
    auto buffer =  std::make_shared<payload_buffer>();
    auto packet = infrastructure::BspPacket{
        5, 6, 7, 20
    };
    auto buffer_size = packet.PackFrame(payload, buffer);
    auto packet_new = infrastructure::BspPacket::TryParseFrame(buffer, buffer_size);

    REQUIRE_EQ(packet_new->id, 5);
    REQUIRE_EQ(packet_new->sequence_number, 6);
    REQUIRE_EQ(packet_new->session_number, 7);
    REQUIRE_EQ(packet_new->timestamp, 20);
    REQUIRE_EQ(packet_new->data_length, payload_in.size());

    std::ostringstream convert;
    for (int a = 0; a < packet_new->data_length; a++) {
        convert << *(packet_new->data_start + a);
    }
    std::string payload_out = convert.str();

    REQUIRE_EQ(payload_out, payload_in);
}

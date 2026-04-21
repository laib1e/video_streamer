#pragma once
#include <cstdint>
#include <array>
#include <cstring>
#include <arpa/inet.h>

struct RtpHeader {
    uint8_t  version      = 2;
    uint8_t  payload_type = 96;   // dynamic, for video
    uint16_t seq_number   = 0;
    uint32_t timestamp    = 0;
    uint32_t ssrc         = 0x12345678;

    std::array<uint8_t, 12> serialize() const {
        std::array<uint8_t, 12> buf{};
        buf[0] = (version << 6);           // V=2, P=0, X=0, CC=0
        buf[1] = payload_type;             // M=0, PT=96
        uint16_t seq_net = htons(seq_number);
        std::memcpy(&buf[2], &seq_net, 2);
        uint32_t ts_net = htonl(timestamp);
        std::memcpy(&buf[4], &ts_net, 4);
        uint32_t ssrc_net = htonl(ssrc);
        std::memcpy(&buf[8], &ssrc_net, 4);
        return buf;
    }
};
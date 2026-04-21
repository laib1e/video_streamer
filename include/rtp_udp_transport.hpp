#pragma once
#include "rtp_packet.hpp"
#include <span>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

class RtpUdpTransport {
    int sock_ = -1;
    sockaddr_in dest_{};
    RtpHeader header_{};
    std::vector<uint8_t> packet_buf_;  // reusable buffer, no alloc in hot path

public:
    bool open(const char* ip, uint16_t port) {
        sock_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_ < 0) return false;

        dest_.sin_family = AF_INET;
        dest_.sin_port = htons(port);
        if (inet_pton(AF_INET, ip, &dest_.sin_addr) <= 0) {
            ::close(sock_);
            sock_ = -1;
            return false;
        }
        return true;
    }

    bool send(std::span<const uint8_t> payload) {
        if (sock_ < 0) return false;

        auto rtp = header_.serialize();
        packet_buf_.resize(rtp.size() + payload.size());
        std::memcpy(packet_buf_.data(), rtp.data(), rtp.size());
        std::memcpy(packet_buf_.data() + rtp.size(), 
                     payload.data(), payload.size());

        ssize_t sent = sendto(sock_, packet_buf_.data(), packet_buf_.size(),
                              0, (sockaddr*)&dest_, sizeof(dest_));

        header_.seq_number++;
        header_.timestamp += 3000;  // 90kHz / 30fps

        return sent > 0;
    }

    void close() {
        if (sock_ >= 0) {
            ::close(sock_);
            sock_ = -1;
        }
    }

    ~RtpUdpTransport() { close(); }

    RtpUdpTransport() = default;
    RtpUdpTransport(const RtpUdpTransport&) = delete;
    RtpUdpTransport& operator=(const RtpUdpTransport&) = delete;
    RtpUdpTransport(RtpUdpTransport&& other) noexcept
        : sock_(other.sock_), dest_(other.dest_), header_(other.header_), packet_buf_(std::move(other.packet_buf_)) 
    {
        other.sock_ = -1;
    }

    RtpUdpTransport& operator=(RtpUdpTransport&& other) noexcept 
    {
        if (this != &other) {
            close();
            sock_ = other.sock_;
            dest_ = other.dest_;
            header_ = other.header_;
            packet_buf_ = std::move(other.packet_buf_);
            other.sock_ = -1;
        }
        return *this;
    }
};

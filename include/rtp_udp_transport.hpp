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
#include <algorithm>

class RtpUdpTransport 
{
    int sock_ = -1;
    sockaddr_in dest_{};
    RtpHeader header_{};
    std::vector<uint8_t> packet_buf_;  // reusable buffer, no alloc in hot path

public:
    bool open(const char* ip, uint16_t port) 
    {
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

    bool send(std::span<const uint8_t> payload) 
    {
        if (sock_ < 0 and payload.size()) return false;

        auto rtp = header_.serialize();

        ssize_t sent = 0;
        for (size_t i = 0; i < payload.size(); i += 1024) 
        {
            size_t chunk_size = std::min(std::size_t(1024), payload.size() - i);
            if (i + chunk_size >= payload.size()) 
            {
                header_.marker = true;
            }
            std::span<const uint8_t> chunk = payload.subspan(i, chunk_size);

            packet_buf_.resize(rtp.size() + chunk.size());
            std::memcpy(packet_buf_.data(), rtp.data(), rtp.size());
            std::memcpy(packet_buf_.data() + rtp.size(), chunk.data(), chunk.size());
            sent = sendto(sock_, packet_buf_.data(), packet_buf_.size(), 0, (sockaddr*)&dest_, sizeof(dest_));
            if (sent < 0) 
            {
                std::string error_str = std::format("SEND ERROR {}, {}\n", errno, strerror(errno));
                std::printf(error_str.data());
            }
            header_.seq_number++;
        }
        header_.timestamp += 3000;  // 90kHz / 30fps
        header_.marker = false;

        return sent > 0;
    }

    void close() 
    {
        if (sock_ >= 0) 
        {
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
        if (this != &other) 
        {
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

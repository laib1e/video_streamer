#pragma once
#include "header.hpp"
#include "H264Parser.hpp"

#include <span>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <optional>
#include <string>
#include <format>

class RTPServer 
{
public:
	bool open(const char* ip, uint16_t port);
	bool send(std::span<const uint8_t> payload);
    void close();

    RTPServer() = default;
	~RTPServer();

    RTPServer(const RTPServer&) = delete;
    RTPServer& operator=(const RTPServer&) = delete;
    RTPServer(RTPServer&& other) noexcept;
    RTPServer& operator=(RTPServer&& other) noexcept;

private:
    int sock_ = -1;
    sockaddr_in dest_{};
    RTPHeader header_{};
    H264Parser parser_;
    std::vector<uint8_t> packet_buf_;
    uint32_t fps_ = 30;
    uint32_t timestamp_step_ = 3000;
	const int max_payload = 1200;
};
#include "RTPServer.hpp"
#include <random>

bool RTPServer::open(const char* ip, uint16_t port) 
{
    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ < 0) return false;

    dest_.sin_family = AF_INET;
    dest_.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &dest_.sin_addr) <= 0) 
    {
        ::close(sock_);
        sock_ = -1;
        return false;
    }
    return true;
}

uint32_t RTPServer::generate_initial_timestamp()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint32_t> dist;
    return dist(gen);
}

uint32_t RTPServer::make_rtp_timestamp(uint64_t frame_timestamp_us)
{
    if (!timestamp_started_) 
    {
        first_frame_timestamp_us_ = frame_timestamp_us;
        initial_rtp_timestamp_ = generate_initial_timestamp();
        timestamp_started_ = true;
    }

    const uint64_t elapsed_us = frame_timestamp_us - first_frame_timestamp_us_;

    return initial_rtp_timestamp_ + static_cast<uint32_t>((elapsed_us * rtp_clock_rate_) / 1'000'000ULL);
}

ssize_t RTPServer::RTP_Packetizer(std::span<const uint8_t> payload) noexcept
{
    auto rtp = header_.serialize();
    packet_buf_.resize(rtp.size() + payload.size());
    std::memcpy(packet_buf_.data(), rtp.data(), rtp.size());
    std::memcpy(packet_buf_.data() + rtp.size(), payload.data(), payload.size());
    return sendto(sock_, packet_buf_.data(), packet_buf_.size(), 0, (sockaddr*)&dest_, sizeof(dest_));
}

bool RTPServer::send(std::span<const uint8_t> payload, uint64_t frame_timestamp_us) 
{
    ssize_t sent = 0;
    if (sock_ < 0 or payload.empty()) return false;
    
    header_.timestamp = make_rtp_timestamp(frame_timestamp_us);
    // debug
    // static uint32_t last_ts = 0;
    // static bool has_last = false;

    // if (has_last) {
    //     uint32_t delta = header_.timestamp - last_ts;
    //     std::printf("rtp ts delta=%u\n", delta);
    // }

    // last_ts = header_.timestamp;
    // has_last = true;
    // debug
    NALUnit nals = parser_(payload);
    
    if (nals.empty())
        return false;

    for (size_t i = 0; i < nals.size(); ++i) 
    {
        auto nal = nals[i];
        bool is_last_nal = ((i + 1) == nals.size());

        if (nal.size() <= max_payload) 
        {
            header_.marker = is_last_nal;
            sent = RTP_Packetizer(nal);
            if (sent < 0) 
            {
                std::string error_str = std::format("SEND ERROR {}, {}\n", errno, strerror(errno));
                std::printf(error_str.data());
            }
            header_.seq_number++;
        } else {
            parser_.FU_A_Packetizer(nal, is_last_nal, [&](std::span<const uint8_t> nal, bool marker) {
                header_.marker = marker;
                sent = RTP_Packetizer(nal);
                if (sent < 0) 
                {
                    std::string error_str = std::format("SEND ERROR {}, {}\n", errno, strerror(errno));
                    std::printf(error_str.data());
                }
                header_.seq_number++;
            });
        }
    }
    return sent > 0;
}

void RTPServer::close() 
{
    if (sock_ >= 0) 
    {
        ::close(sock_);
        sock_ = -1;
    }
}

RTPServer::~RTPServer() { close(); }
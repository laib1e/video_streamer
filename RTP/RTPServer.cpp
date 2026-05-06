#include "RTPServer.hpp"

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
    timestamp_step_ = 90000 / fps_;
    return true;
}

bool RTPServer::send(std::span<const uint8_t> payload) 
{
    ssize_t sent = 0;
    if (sock_ < 0 or payload.empty()) return false;

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
            auto rtp = header_.serialize();
            packet_buf_.resize(rtp.size() + nal.size());
            std::memcpy(packet_buf_.data(), rtp.data(), rtp.size());
            std::memcpy(packet_buf_.data() + rtp.size(), nal.data(), nal.size());
            sent = sendto(sock_, packet_buf_.data(), packet_buf_.size(), 0, (sockaddr*)&dest_, sizeof(dest_));
            if (sent < 0) 
            {
                std::string error_str = std::format("SEND ERROR {}, {}\n", errno, strerror(errno));
                std::printf(error_str.data());
            }
            header_.seq_number++;
        } else {
            parser_.FU_A_Packetizer(nal, is_last_nal, [&](std::span<const uint8_t> nal, bool marker) {
                header_.marker = marker;
                auto rtp = header_.serialize();
                packet_buf_.resize(rtp.size() + nal.size());
                std::memcpy(packet_buf_.data(), rtp.data(), rtp.size());
                std::memcpy(packet_buf_.data() + rtp.size(), nal.data(), nal.size());
                sent = sendto(sock_, packet_buf_.data(), packet_buf_.size(), 0, (sockaddr*)&dest_, sizeof(dest_));
                if (sent < 0) 
                {
                    std::string error_str = std::format("SEND ERROR {}, {}\n", errno, strerror(errno));
                    std::printf(error_str.data());
                }
                header_.seq_number++;
            });
        }
    }

    header_.timestamp += timestamp_step_;
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
RTPServer::RTPServer(RTPServer&& other) noexcept
    : sock_(other.sock_), dest_(other.dest_), header_(other.header_), packet_buf_(std::move(other.packet_buf_)) 
{
    other.sock_ = -1;
}

RTPServer &RTPServer::operator=(RTPServer &&other) noexcept
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
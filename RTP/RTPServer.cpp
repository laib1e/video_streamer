#include "RTPServer.hpp"


struct start_code 
{
    size_t posision = 0;
    size_t length   = 0;
};

static std::optional<start_code> find_start_h264(std::span<const uint8_t> payload, const int offset) 
{
    if (payload.size() < 3 || offset >= payload.size())
        return std::nullopt;

    for(size_t i = offset; i < payload.size() - 3; ++i) 
    {
        if (payload[i] != 0x00)
            continue;


        if (i + 3 < payload.size() and 
            payload[i] == 0x00 and 
            payload[i + 1] == 0x00 and
            payload[i + 2] == 0x00 and
            payload[i + 3] == 0x01) 
        {
            return start_code{i, 4};
        }
        
        if (payload[i] == 0x00 and
                    payload[i + 1] == 0x00 and
                    payload[i + 2] == 0x01)
        {
            return start_code{i, 3};
        }
    }
    return std::nullopt;
}

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
    if (sock_ < 0 or payload.empty()) return false;

    auto start_code_position = find_start_h264(payload, 0);
    ssize_t sent = 0;

    if (not start_code_position) 
        return false;

    while (start_code_position)
    {
        size_t nal_begin = start_code_position->posision + start_code_position->length;
        auto next_start_code_position = find_start_h264(payload, nal_begin);

        size_t nal_end = next_start_code_position ? next_start_code_position->posision : payload.size();

        if (nal_end <= nal_begin)
        {
            start_code_position = next_start_code_position;
            continue;
        }

        std::span<const uint8_t> nal = payload.subspan(nal_begin, nal_end - nal_begin);
        int nal_type = nal[0] & 0x1F;
        if (nal.size() <= 1200) 
        {
            header_.marker = not next_start_code_position;
            auto rtp = header_.serialize();
            packet_buf_.resize(rtp.size() + nal.size());
            std::memcpy(packet_buf_.data(), rtp.data(), rtp.size());
            std::memcpy(packet_buf_.data() + rtp.size(), nal.data(), nal.size());
            sent = sendto(sock_, packet_buf_.data(), packet_buf_.size(), 0, (sockaddr*)&dest_, sizeof(dest_));
            if (sent < 0) 
            {
                std::string error_str = std::format("SEND ERROR {}, {}\n", errno, strerror(errno));
                std::printf(error_str.data());
                return false;
            }
            header_.seq_number++;
        } else {

            uint8_t nal_header = nal[0];

            uint8_t fu_indicator = (nal_header & 0xE0) | 28;
            uint8_t type = nal_header & 0x1F;

            std::span<const uint8_t> nal_payload = nal.subspan(1, nal.size() - 1);

            for (size_t i = 0; i < nal_payload.size(); i += 1200 - 2) 
            {
                size_t chunk_size = std::min(std::size_t(1200 - 2), nal_payload.size() - i);
                uint8_t header = type;
                header_.marker = false;
                if (i + chunk_size >= nal_payload.size()) 
                {
                    header |= 0x40;
                    header_.marker = not next_start_code_position;
                } else if (i == 0) {
                    header |= 0x80;
                }
                std::span<const uint8_t> chunk = nal_payload.subspan(i, chunk_size);

                auto rtp = header_.serialize();
                packet_buf_.resize(rtp.size() + chunk.size() + 2);
                std::memcpy(packet_buf_.data(), rtp.data(), rtp.size());

                packet_buf_[rtp.size()] = fu_indicator;
                packet_buf_[rtp.size() + 1] = header;
                std::memcpy(packet_buf_.data() + rtp.size() + 2, chunk.data(), chunk.size());
                sent = sendto(sock_, packet_buf_.data(), packet_buf_.size(), 0, (sockaddr*)&dest_, sizeof(dest_));
                if (sent < 0) 
                {
                    std::string error_str = std::format("SEND ERROR {}, {}\n", errno, strerror(errno));
                    std::printf(error_str.data());
                    return false;
                }
                header_.seq_number++;
            }
        }

        if (not next_start_code_position)
            break;

        start_code_position = find_start_h264(payload, next_start_code_position->posision);
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
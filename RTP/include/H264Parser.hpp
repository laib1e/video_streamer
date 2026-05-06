#pragma once

#include <cstdint>
#include <vector>
#include <span>
#include <optional>
#include <functional>

struct H264StartCode
{
    size_t posision = 0;
    size_t length   = 0;
};

using NALUnit = std::vector<std::span<const uint8_t>>;
using PacketizerCallback = std::function<void(std::span<const uint8_t>, bool)>;

class H264Parser 
{
public:
	H264Parser() = default;
	~H264Parser() = default;

	NALUnit operator()(std::span<const uint8_t> payload) 
	{
		NALUnit result;
		auto start_code_position = find_start_code(payload, 0);

		while (start_code_position) 
		{
			size_t nal_begin = 0;
			size_t nal_end = 0;

			nal_begin = start_code_position->posision + start_code_position->length;
			auto next_start_code_position = find_start_code(payload, nal_begin);
			nal_end = next_start_code_position ? next_start_code_position->posision : payload.size();
		
			if (nal_end <= nal_begin)
			{
				start_code_position = next_start_code_position;
				continue;
			}
			
			result.emplace_back(payload.subspan(nal_begin, nal_end - nal_begin));
			start_code_position = next_start_code_position;
		}
		
		return result;
	}

	void FU_A_Packetizer(std::span<const uint8_t> payload, bool last, PacketizerCallback func)  
	{
		const uint8_t payload_header = payload[0];
		const uint8_t fu_indicator = (payload_header & 0xE0) | 28;
		const uint8_t nal_type = (payload_header & 0x1F);
		
		payload = payload.subspan(1);
		std::vector<uint8_t> result;
		
		for (size_t offset = 0; offset < payload.size(); offset += (max_payload - 2)) 
		{
			bool marker = false;

			const size_t chunk_size = std::min(std::size_t(max_payload - 2), payload.size() - offset);
			const bool start = offset == 0;
			const bool end = offset + chunk_size >= payload.size();

			uint8_t fu_header = nal_type;
			if (start) 
				fu_header |= 0x80;
			if (end) 
				fu_header |= 0x40;

			result.resize(chunk_size + 2);
			result[0] = fu_indicator;
			result[1] = fu_header;
			auto chunk = payload.subspan(offset, chunk_size);
			std::memcpy(result.data() + 2, chunk.data(), chunk.size());
			func(std::span<const uint8_t>(result), last and end);
		}
	}

private:
	const int max_payload = 1200;

	static std::optional<H264StartCode> find_start_code(std::span<const uint8_t> payload, std::size_t offset) noexcept
	{
		if (payload.size() < 3 || offset >= payload.size())
			return std::nullopt;

		for(size_t i = offset; i + 2 < payload.size(); ++i) 
		{
			if (payload[i] != 0x00)
				continue;

			if (i + 3 < payload.size() and 
				payload[i] == 0x00     and 
				payload[i + 1] == 0x00 and
				payload[i + 2] == 0x00 and
				payload[i + 3] == 0x01) 
			{
				return H264StartCode{i, 4};
			}
			
			if (payload[i] == 0x00     and
				payload[i + 1] == 0x00 and
				payload[i + 2] == 0x01)
			{
				return H264StartCode{i, 3};
			}
		}
		return std::nullopt;
	}
};
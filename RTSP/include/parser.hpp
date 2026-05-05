#pragma once

#include <optional>
#include <charconv>
#include <string_view>
#include <cstdint>

struct transport_params 
{
	uint16_t client_rtp_port;
	uint16_t client_rtcp_port;
};

struct request 
{
	std::string_view method;
	std::string_view uri;
	std::string_view version;

	int cseq = -1;

	std::string_view transport;
	std::string_view session;
	std::string_view user_agent;
};

inline constexpr bool is_space(char c) noexcept 
{
	return c == ' ' or c == '\t' or c == '\n' or c == '\r';
}

inline constexpr std::string_view clean(std::string_view s) noexcept 
{
	while (not s.empty() and is_space(s.front()))
		s.remove_prefix(1);

	while (not s.empty() and is_space(s.back()))
		s.remove_suffix(1);

	return s;
}

inline constexpr std::string_view next_line(std::string_view& input) noexcept 
{
	const auto pos = input.find('\n');

	if (pos == std::string_view::npos)
	{
		auto line = input;
		input = {};
		return clean(line);
	}

	auto line = input.substr(0, pos);
	input.remove_prefix(pos + 1);
	return clean(line);
}

inline constexpr std::string_view next_token(std::string_view& input, char delit) noexcept 
{
	input = clean(input);

	const auto pos = input.find(delit);

	if (pos == std::string_view::npos) 
	{
		auto token = input;
		input = {};
		return token;
	}

	auto token = input.substr(0, pos);
	input.remove_prefix(pos + 1);
	return clean(token);
}

inline std::optional<int> parse_int(std::string_view input) noexcept
{
	input = clean(input);

	int value = 0;
	const char* begin = input.data();
	const char* end   = input.data() + input.size();

	auto [ptr, error] = std::from_chars(begin, end, value);

	if (error != std::errc() or ptr != end)
		return std::nullopt;

	return value;
}

inline std::optional<uint16_t> parse_port(std::string_view input) noexcept
{
    input = clean(input);

    if (input.empty())
        return std::nullopt;

    unsigned int value = 0;

    auto* begin = input.data();
    auto* end = input.data() + input.size();

    auto [ptr, ec] = std::from_chars(begin, end, value);

    if (ec != std::errc{} || ptr != end)
        return std::nullopt;

    if (value == 0 || value > 65535)
        return std::nullopt;

    return static_cast<uint16_t>(value);
}

inline std::optional<transport_params> parse_transport_header(std::string_view line) noexcept 
{
	transport_params result;

	while(not line.empty()) 
	{
		auto part = next_token(line, ';');

		if (part.empty())
			continue;

		if (part.starts_with("client_port=")) 
		{
			auto value = part.substr(std::string_view{"client_port="}.size());
			const auto dash = value.find('-');

			if (dash == std::string_view::npos) 
			{
				auto rtp_port = parse_port(value);
				if (not rtp_port)
					return std::nullopt;

				result.client_rtp_port = *rtp_port;

				if (*rtp_port == 65535)
					return std::nullopt;
				
				result.client_rtcp_port = static_cast<uint16_t>(*rtp_port + 1);
			} else {
				auto rtp_port = parse_port(clean(value.substr(0, dash)));
				auto rtcp_port = parse_port(clean(value.substr(dash + 1)));

				if (not rtp_port or not rtcp_port)
					return std::nullopt;

				result.client_rtp_port = *rtp_port;
				result.client_rtcp_port = *rtcp_port;
			}
		}
	}

	if (result.client_rtp_port == 0 or result.client_rtcp_port == 0)
		return std::nullopt;

	return result;
}

inline std::optional<request> parser(std::string_view raw) noexcept 
{
	request result;

	auto line = next_line(raw);

	if (line.empty())
		return std::nullopt;
	
	result.method  = next_token(line, ' ');
	result.uri 	   = next_token(line, ' ');
	result.version = next_token(line, ' ');

	if (result.method.empty() or result.uri.empty() or result.version.empty())
		return std::nullopt;

	if (result.version != "RTSP/1.0")
		return std::nullopt;

	while (not raw.empty())
	{
		auto line = next_line(raw);

		const auto caret = line.find(':');
		if (caret == std::string_view::npos)
			continue;

		const auto key = clean(line.substr(0, caret));
		const auto value = clean(line.substr(caret + 1));

		if (key == "CSeq")
		{

			auto parsed = parse_int(value);
			if (not parsed)
				return std::nullopt;

			result.cseq = *parsed;
		} else if (key == "Transport") {
			result.transport = value;
		} else if (key == "Session") {
			result.session = value;
		} else if (key == "User-Agent") {
			result.user_agent = value;
		}
	}

	if (result.cseq < 0)
		return std::nullopt;

	return result;
}
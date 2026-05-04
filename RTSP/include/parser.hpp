#pragma once
#include "request.hpp"

#include <optional>
#include <charconv>

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

inline constexpr std::string_view next_token(std::string_view& input) noexcept 
{
	input = clean(input);

	const auto pos = input.find(' ');

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

inline constexpr std::optional<int> parse_int(std::string_view input) noexcept
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

inline std::optional<request> parser(std::string_view raw) noexcept 
{
	request result;

	auto line = next_line(raw);

	if (line.empty())
		return std::nullopt;
	
	result.method  = next_token(line);
	result.uri 	   = next_token(line);
	result.version = next_token(line);

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
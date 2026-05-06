#pragma once

enum class RTSPState 
{
	Init,
	Ready,
	Playing,
	Closing
}; 

struct client_ctx
{
	std::string in_buf;
	std::string out_buf;

	std::string session_id;
	std::string address;
	uint16_t rtp_port = 0;
	uint16_t rtcp_port = 0;

	RTSPState state = RTSPState::Init;

	client_ctx(size_t res, const char* ip) : address(ip)
	{
		in_buf.reserve(res);
		out_buf.reserve(res);
	}
};
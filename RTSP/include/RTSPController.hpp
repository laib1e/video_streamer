#pragma once
#include "ResponseBuilder.hpp"
#include "client_ctx.hpp"
#include "parser.hpp"

template <typename Streamer>
class RTSPController 
{
public:
	explicit RTSPController(Streamer& streamer) : streamer_(streamer) {};
	~RTSPController() = default;

	std::string process(std::string_view raw, client_ctx& client) 
	{
		const auto parsed = parser(raw);

		if (not parsed) 
		{
			return 
				"RTSP/1.0 400 Bad Request\r\n"
				"\r\n";
		}

		const auto& request = *parsed;

		if (request.method == "OPTIONS") 
		{
			return handle_options(request, client);
		}

		if (request.method == "DESCRIBE") 
		{
			return handle_describe(request, client);
		}

		if (request.method == "SETUP") 
		{
			return handle_setup(request, client);
		}

		if (request.method == "PLAY")
		{
			return handle_play(request, client);
		}

		if (request.method == "TEARDOWN") 
		{
			return handle_teardown(request, client);
		}

		return ResponseBuilder(405, "Method Not Allowed", request.cseq)
		.header("Public", "OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN")
		.build();
	};

private:
	Streamer& streamer_;
	uint64_t next_session_id_ = 1; 

	std::string handle_options(const request& request, client_ctx& client) 
	{
		return ResponseBuilder(200, "OK", request.cseq)
		.header("Public", "OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN")
		.build();
	}

	std::string handle_describe(const request& request, client_ctx& client) 
	{
		return ResponseBuilder(200, "OK", request.cseq)
			.header("Content-Type", "application/sdp")
			.build(
				"v=0\r\n"
				"o=- 0 0 IN IP4 0.0.0.0\r\n"
				"s=video_streamer\r\n"
				"c=IN IP4 0.0.0.0\r\n"
				"t=0 0\r\n"
				"a=control:*\r\n"
				"m=video 0 RTP/AVP 96\r\n"
				"a=rtpmap:96 H264/90000\r\n"
				"a=fmtp:96 packetization-mode=1\r\n"
				"a=control:trackID=0\r\n"
			);
	}

	std::string handle_setup(const request& request, client_ctx& client) 
	{
		if (request.transport.empty()) 
		{
			return ResponseBuilder(461, "Unsupported Transport", request.cseq).build();
		}

		const auto parse_transport = parse_transport_header(request.transport);
		if (not parse_transport) 
		{
			return ResponseBuilder(461, "Unsupported Transport", request.cseq).build();
		}

		if (not streamer_.set_options(client.address.data(), parse_transport->client_rtp_port)) 
		{
			return ResponseBuilder(500, "Internal Server Error", request.cseq).build();
		}

		client.rtcp_port = parse_transport->client_rtcp_port;
		client.rtp_port  = parse_transport->client_rtp_port;
		client.session_id = std::to_string(++next_session_id_);
		client.state = RTSPState::Ready;

		return ResponseBuilder(200, "OK", request.cseq)
		.header("Transport", request.transport)
		.header("Session", client.session_id)
		.build();
	}

	std::string handle_play(const request& request, client_ctx& client) 
	{
		if (client.state != RTSPState::Ready and client.state != RTSPState::Playing) 
		{
			return ResponseBuilder(455, "Method Not Valid in This State", request.cseq).build();
		}

		if (request.session.empty() or request.session != client.session_id) 
		{
			return ResponseBuilder(454, "Session Not Found", request.cseq).build();
		}
			
		streamer_.start();
		client.state = RTSPState::Playing;

		return ResponseBuilder(200, "OK", request.cseq)
		.header("Session", client.session_id)
		.build();
	}

	std::string handle_teardown(const request& request, client_ctx& client) 
	{
		streamer_.stop();
		std::string session = client.session_id;
		client.state = RTSPState::Closing;
		client.session_id = "";
		return ResponseBuilder(200, "OK", request.cseq)
		.header("Session", session)
		.build();
	}
};

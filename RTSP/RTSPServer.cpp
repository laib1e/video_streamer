#include "RTSPServer.hpp"

static std::string process(std::string_view raw) 
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
		return ResponseBuilder(200, "OK", request.cseq)
			.header("Public", "OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN")
			.build();

	if (request.method == "DESCRIBE")
		return ResponseBuilder(200, "OK", request.cseq)
			.header("Content-Type", "application/sdp")
			.build(
				"v=0\r\n"
				"o=- 0 0 IN IP4 0.0.0.0\r\n"
				"s=video_streamer\r\n"
				"a=control:*\r\n"
				"m=video 0 RTP/AVP 96\r\n"
				"a=rtpmap:96 H264/90000\r\n"
				"a=fmtp:96 packetization-mode=1\r\n"
				"a=control:trackID=0\r\n"
			);

	if (request.method == "SETUP")
		return ResponseBuilder(501, "Not Implemented", request.cseq).build();
	
	if (request.method == "PLAY")
		return ResponseBuilder(501, "Not Implemented", request.cseq).build();

	if (request.method == "TEARDOWN")
		return ResponseBuilder(501, "Not Implemented", request.cseq).build();

	
	return ResponseBuilder(405, "Methow Not Allowed", request.cseq)
		.header("Public", "OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN")
		.build();
};

RTSPServer::RTSPServer(uint16_t port) : port_(port) 
{
	sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock_ == -1) throw std::runtime_error("OPEN TCP SOCKET ERROR");
	{
		int opt = 1;
		setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	}
	sockaddr_in address;
	{
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(port);
	}
	if (bind(sock_, (struct sockaddr*)&address, sizeof(address)) < 0) 
	{
		::close(sock_);
		throw std::runtime_error("BIND TCP FAILED");
	}
	listen(sock_, 1);
}

void RTSPServer::run() 
{
	int client_sock = accept(sock_, nullptr, nullptr);
	if (client_sock == -1) return;

	char buf[4096];
	int n = read(client_sock, buf, sizeof(buf));
	if (n <= 0) 
	{
		::close(client_sock);
	} else {
		std::string_view request(buf, n);
		std::string response = process(request);
		write(client_sock, response.data(), response.size());
	}
};

void RTSPServer::close() 
{
	if (sock_ >= 0) 
	{
		::close(sock_);
		sock_ = -1;
	}
};

RTSPServer::RTSPServer(RTSPServer&& other) noexcept
	: sock_(other.sock_), port_(other.port_)
{
	other.sock_ = -1;
}

RTSPServer& RTSPServer::operator=(RTSPServer&& other) noexcept 
{
	if (this != &other) 
	{
		close();
		sock_ = other.sock_;
		port_ = other.port_;
		other.sock_ = -1;
	}
	return *this;
}

RTSPServer::~RTSPServer() 
{
	close();
};
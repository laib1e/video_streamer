#pragma once
#include "ResponseBuilder.hpp"
#include "parser.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <vector>
#include <stdexcept>
#include <fcntl.h>
#include <algorithm>

enum State 
{
	None,
	Init,
	Ready,
	Playning
}; 

struct client_ctx
{
	int fd;
	std::string in_buf;
	std::string out_buf;

	std::string session_id;
	std::string address;
	uint16_t rtp_port;
	uint16_t rtcp_port;

	State state = None;

	client_ctx(int s, size_t res, const char* ip) : fd(s), address(ip)
	{
		in_buf.reserve(res);
	}
};

template <typename Streamer, int MaxClient = 10>
class RTSPServer 
{
public:
	explicit RTSPServer(uint16_t port, Streamer& streamer) : port_(port), streamer_(streamer)
	{
		sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock_ == -1) throw std::runtime_error("OPEN TCP SOCKET ERROR");
		{
			int opt = 1;
			int flags = fcntl(sock_, F_GETFL, 0);
			setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
			fcntl(sock_, F_SETFL, flags | O_NONBLOCK);
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
		listen(sock_, SOMAXCONN);

		epoll_fd_ = epoll_create1(0);
		struct epoll_event ev{};
		ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
		ev.data.fd = sock_;
		epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, sock_, &ev);
	};
	
	void run() 
	{
		epoll_event events[MaxClient];
		int wait = epoll_wait(epoll_fd_, events, MaxClient, -1);
		for (size_t i = 0; i < wait; i++) 
		{
			int fd = events[i].data.fd;
			uint32_t revents = events[i].events;

			if (revents & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) 
			{
				epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
				::close(fd);

				auto it = std::find_if(clients_.begin(), clients_.end(), [fd](const auto& client) { return client.fd == fd; });
				if (it != clients_.end()) 
				{
					std::iter_swap(it, clients_.end() - 1);
					clients_.pop_back();
				}
				continue;
			} 

			if (revents & EPOLLOUT) 
			{
				auto client = std::ranges::find_if(clients_, [fd](const auto& client) { return client.fd == fd; });
				if (client == clients_.end())
				{
					continue;
				}
				
				if (not client->out_buf.empty()) 
				{
					ssize_t n = send(client->fd, client->out_buf.data(), client->out_buf.size(), MSG_NOSIGNAL);

					if (n > 0) 
					{
						client->out_buf.erase(0, n);
					} else if (n == -1 and errno != EAGAIN and errno != EWOULDBLOCK) {
						epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
						::close(fd);
						clients_.erase(client);
					}
				} else {
					epoll_event ev{};
					ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
					ev.data.fd = client->fd;
					epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client->fd, &ev);
				}
				continue;
			}

			if (fd == sock_) 
			{
				while (true) 
				{
					struct sockaddr_in addr;
					socklen_t addr_size = sizeof(addr);
					int client_sock = accept(sock_, (struct sockaddr*)&addr, &addr_size);
					if (client_sock == -1) 
					{
						if (errno == EAGAIN or errno == EWOULDBLOCK) 
						{
							break;
						} else {
							continue;
						}
					}
					{
						int flags = fcntl(client_sock, F_GETFL, 0);
						fcntl(client_sock, F_SETFL, flags | O_NONBLOCK);
					}
					struct epoll_event ev{};
					ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
					ev.data.fd = client_sock;
					epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, client_sock, &ev);
					char ip[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
					clients_.emplace_back(client_sock, 4096, ip);
				}
			} else {
				auto client = std::ranges::find_if(clients_, [fd](const auto& client) { return client.fd == fd; });
				if (client != clients_.end())
				{
					while (true) 
					{
						char buf[4096];
						int n = read(fd, buf, sizeof(buf));
						if (n > 0) 
						{
							client->in_buf.append(buf, n);
						} else if (n < 0 and (errno == EAGAIN or errno == EWOULDBLOCK)) {
							break;
						} else {
							epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
							::close(fd);
							clients_.erase(client);
							break;
						}
					}

					while (true) 
					{				
						const std::string request_end = "\r\n\r\n";
						auto it = std::search(client->in_buf.begin(), client->in_buf.end(), request_end.begin(), request_end.end());
						if (it == client->in_buf.end())
						{
							break;
						}

						size_t request_size = std::distance(client->in_buf.begin(), it) + request_end.size();
						std::string request(client->in_buf.begin(), client->in_buf.begin() + request_size);

						auto response = process(request, *client);
						client->out_buf += response;
						{
							epoll_event ev{};
							ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
							ev.data.fd = client->fd;
							epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, client->fd, &ev);
						}

						client->in_buf.erase(0, request_size);
					}
				}
				continue;
			}
		}
	};

	void close() 
	{
		if (sock_ >= 0) 
		{
			::close(sock_);
			sock_ = -1;
		}
	};

	~RTSPServer()
	{
		close();
	};

	RTSPServer(const RTSPServer&) = delete;
	RTSPServer& operator=(const RTSPServer&) = delete;
	
	RTSPServer(RTSPServer&& other) noexcept 
	: sock_(other.sock_), port_(other.port_)
	{
		other.sock_ = -1;
	};

	RTSPServer& operator=(RTSPServer&& other) noexcept
	{
		if (this != &other) 
		{
			close();
			sock_ = other.sock_;
			port_ = other.port_;
			other.sock_ = -1;
		}
		return *this;
	};

private:
	int sock_ = -1;
	int epoll_fd_ = -1;
	uint16_t port_ = -1;
	Streamer& streamer_;
	std::vector<client_ctx> clients_;

	std::string process(std::string_view raw, client_ctx &client) 
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
			client.state = Init;
			return ResponseBuilder(200, "OK", request.cseq)
			.header("Public", "OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN")
			.build();
		}
		
		if (request.method == "DESCRIBE") 
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


		if (request.method == "SETUP") 
		{
			if (not request.transport.empty()) 
			{
				const auto parse_transport = parse_transport_header(request.transport);
				if (not parse_transport) 
				{
					return 
						"RTSP/1.0 461 Unsupported Transport\r\n"
						"\r\n";
				}
				client.rtcp_port = parse_transport->client_rtcp_port;
				client.rtp_port  = parse_transport->client_rtp_port;
				client.session_id = "1245678";
				client.state = streamer_.set_options(client.address.data(), client.rtp_port) ? Ready : None;

				return ResponseBuilder(200, "OK", request.cseq)
				.header("Transport", std::string(request.transport) + ";server_port=5004-5005")
				.header("Session", client.session_id)
				.build();
			} else {
				return 
					"RTSP/1.0 461 Unsupported Transport\r\n"
					"\r\n";
			}
		}
		
		if (request.method == "PLAY")
		{
			if (client.state == Ready) 
			{
				if (request.session.empty()) 
				{
					return ResponseBuilder(454, "Session Not Found", request.cseq).build();
				} 
				client.state = Playning;
				streamer_.start();

				return ResponseBuilder(200, "OK", request.cseq)
				.header("Session", client.session_id)
				.build();
			} else {
				return ResponseBuilder(455, "Method Not Valid in This State", request.cseq).build();
			}
		}

		if (request.method == "TEARDOWN") 
		{
			client.state = Init;
			client.session_id = "";
			streamer_.stop();
			return ResponseBuilder(200, "OK", request.cseq).build();
		}

		
		return ResponseBuilder(405, "Methow Not Allowed", request.cseq)
			.header("Public", "OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN")
			.build();
	}
};
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

struct client_ctx
{
	int fd;
	std::string buf;
	std::string out_buf;

	client_ctx(int s, size_t res) : fd(s) 
	{
		buf.reserve(res);
	}
};

class RTSPServer 
{
public:
	explicit RTSPServer(uint16_t port);
	
	void run();
	void close();

	~RTSPServer();
	RTSPServer(const RTSPServer&) = delete;
	RTSPServer& operator=(const RTSPServer&) = delete;
	
	RTSPServer(RTSPServer&& other) noexcept;
	RTSPServer& operator=(RTSPServer&& other) noexcept;

private:
	int sock_ = -1;
	int epoll_fd_ = -1;
	uint16_t port_ = -1;
	std::vector<client_ctx> clients_;
};
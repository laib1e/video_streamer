#pragma once
#include "ResponseBuilder.hpp"
#include "parser.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <stdexcept>

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
	uint16_t port_ = -1;
};
#pragma once
#include <RTSPController.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdexcept>
#include <fcntl.h>
#include <algorithm>

template <typename Streamer, int MaxClient = 10>
class RTSPServer 
{
public:
	explicit RTSPServer(uint16_t port, Streamer& streamer) : port_(port), streamer_(streamer), controller_(streamer)
	{
		sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock_ == -1) throw std::runtime_error("OPEN TCP SOCKET ERROR");
		{
			int opt = 1;
			int flags = fcntl(sock_, F_GETFL, 0);
			setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
			fcntl(sock_, F_SETFL, flags | O_NONBLOCK);
		}

		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_addr.s_addr = INADDR_ANY;
		address.sin_port = htons(port);

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

			if (fd == sock_) 
			{
				accept_clients();
			} else {
				read_client(fd);
				process_client_buffer(fd);
			}

			if (revents & EPOLLOUT) 
			{
				flush_client(fd);
			}

			if (revents & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) 
			{
				remove_client(fd);
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
	RTSPServer& operator=(RTSPServer&& other) = delete;	
	RTSPServer(RTSPServer&& other) = delete;

private:
	int sock_ = -1;
	int epoll_fd_ = -1;
	int session_id_ = 0;
	uint16_t port_ = -1;
	Streamer& streamer_;
	RTSPController<Streamer> controller_;
	std::unordered_map<int, client_ctx> clients_;

	client_ctx* find_client(int fd) 
	{
		auto it = clients_.find(fd);
		if (it == clients_.end())
			return nullptr;

		return &it->second;
	}

	void accept_clients()
	{
		while (true) 
		{
			struct sockaddr_in addr;
			socklen_t addr_size = sizeof(addr);
			int sock = accept(sock_, (struct sockaddr*)&addr, &addr_size);
			if (sock == -1) 
			{
				if (errno == EAGAIN or errno == EWOULDBLOCK) 
				{
					break;
				}
				continue;
			}
			{
				int flags = fcntl(sock, F_GETFL, 0);
				fcntl(sock, F_SETFL, flags | O_NONBLOCK);
			}
			
			struct epoll_event ev{};
			ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
			ev.data.fd = sock;
			epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, sock, &ev);

			char ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
			clients_.emplace(sock, client_ctx(4096, ip));
		}
	}

	void read_client(int fd_client) 
	{
		auto* client = find_client(fd_client);
		if (not client) return;

		while (true) 
		{
			char buf[4096];
			int n = read(fd_client, buf, sizeof(buf));
			if (n > 0) 
			{
				client->in_buf.append(buf, n);
			} else if (n < 0 and (errno == EAGAIN or errno == EWOULDBLOCK)) {
				break;
			} else {
				remove_client(fd_client);
				break;
			}
		}
	}

	void process_client_buffer(int fd_client) noexcept
	{
		auto* client = find_client(fd_client);
		if (not client) return;

		while (true) 
		{				
			static constexpr std::string_view request_end = "\r\n\r\n";
			auto it = std::search(client->in_buf.begin(), client->in_buf.end(), request_end.begin(), request_end.end());
			if (it == client->in_buf.end())
			{
				break;
			}

			size_t request_size = std::distance(client->in_buf.begin(), it) + request_end.size();
			std::string request(client->in_buf.begin(), client->in_buf.begin() + request_size);
			client->in_buf.erase(0, request_size);

			auto response = controller_.process(request, *client);
			client->out_buf += response;
			
			epoll_event ev{};
			ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;
			ev.data.fd = fd_client;
			epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd_client, &ev);
		}
	}

	void flush_client(int fd_client) 
	{
		auto* client = find_client(fd_client);
		if (not client) return;

		if (not client->out_buf.empty()) 
		{
			ssize_t n = send(fd_client, client->out_buf.data(), client->out_buf.size(), MSG_NOSIGNAL);

			if (n > 0) 
			{
				client->out_buf.erase(0, n);
			} else if (n == -1 and errno != EAGAIN and errno != EWOULDBLOCK) {
				remove_client(fd_client);
			}

		} else {

			if (client->state == RTSPState::Closing) 
			{
				remove_client(fd_client);
			} else {
				epoll_event ev{};
				ev.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLHUP;
				ev.data.fd = fd_client;
				epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd_client, &ev);
			}
		}
	}

	void remove_client(int fd_client) 
	{
		epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd_client, nullptr);
		::close(fd_client);
		clients_.erase(fd_client);
	}
};
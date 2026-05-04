#pragma once

#include <span>
#include <cstdint>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

class FileTransport
{
public:
	bool open(const char* path, uint16_t perm);
	bool send(std::span<const uint8_t> data);
	void close();

	FileTransport() = default;
    FileTransport(const FileTransport&) = delete;
    FileTransport& operator=(const FileTransport&) = delete;
	~FileTransport();

private:
	int fd_ = -1;
};
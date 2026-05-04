#pragma once
#include "FileTransport.hpp"

bool FileTransport::open(const char* path, uint16_t perm) 
{
	close();

	fd_ = ::open(path, 
		O_WRONLY | O_CREAT | O_TRUNC, 
		static_cast<mode_t>(perm)
	);

	if (fd_ < 0)
		return false;

	if (::fchmod(fd_, static_cast<mode_t>(perm)) < 0) 
	{
		close();
		return false;
	}

	return true;
}

bool FileTransport::send(std::span<const uint8_t> data) 
{
	const uint8_t* ptr = data.data();
	size_t left = data.size();

	while (left > 0) 
	{
		ssize_t written = ::write(fd_, ptr, left);

		if (written < 0) 
		{
			if (errno == EINTR)
				continue;

			return false;
		}

		if (written == 0)
			return false;

		ptr += written;
		left -= static_cast<size_t>(written);
	}
	return true;
}

void FileTransport::close() 
{
	if (fd_ >= 0) 
	{
		::close(fd_);
		fd_ = -1;
	}
}

FileTransport::~FileTransport()
{
	close();
}
#pragma once
#include "Frame.hpp"
#include <LockFreeQueue.hpp>
#include <thread>
#include <chrono>
#include <cstdio>

#include <sys/stat.h>
#include <sys/errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <linux/videodev2.h>
#include <cstring>

struct buffer 
{
	void* 	  start;
	size_t 	  length;
};

class Camera 
{
public:
    explicit Camera(LockFreeQueue<Frame, 64>& q, 
                 uint32_t fps = 30, 
                 uint32_t w = 320, uint32_t h = 240,
				 const char* devname = "");

    void start();
    void stop();

    uint32_t frames_take() const;
	uint32_t frames_bad() const;
	uint32_t frames_empty() const;
	
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;
    ~Camera();

private:
    LockFreeQueue<Frame, 64>& queue_;
    std::jthread worker_;
    uint32_t frame_seq_ = 0;
	uint32_t frame_error_ = 0;
	uint32_t frame_empty_ = 0;

    uint32_t target_fps_;
    uint32_t width_;
    uint32_t height_;

	int fd_dev_ = -1;
	std::vector<buffer> buffers;
	unsigned int 		num_buffer;

	void capture_loop(std::stop_token st);
};

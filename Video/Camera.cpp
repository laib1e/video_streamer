#pragma once
#include "Camera.hpp"

struct buffer 
{
	void* 	  start;
	size_t 	  length;
};

void Camera::capture_loop(std::stop_token st) 
{
	using namespace std::chrono;
	auto next_frame = steady_clock::now();
	auto interval = milliseconds(1000 / target_fps_);

	while (!st.stop_requested()) 
	{
		struct v4l2_buffer buf;
		std::memset(&buf, 0, sizeof(buf));
		buf.type 	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory 	= V4L2_MEMORY_MMAP;

		if (ioctl(fd_dev_, VIDIOC_DQBUF, &buf) < 0)
		{
			if (errno == EINTR)
				continue;

			std::string error_str = std::format("VIDIOC_DQBUF: ERROR {}, {}\n", errno, strerror(errno));
			std::printf(error_str.data());
			continue;
		}
		
		if (buf.flags & V4L2_BUF_FLAG_ERROR)
		{
			frame_error_++;
			ioctl(fd_dev_, VIDIOC_QBUF, &buf);
			continue;
		}

		if (buf.bytesused == 0)
		{
			frame_empty_++;
			ioctl(fd_dev_, VIDIOC_QBUF, &buf);
			continue;
		}

		Frame f;
		f.width = width_;
		f.height = height_;
		f.sequence = frame_seq_++;
		f.timestamp_us = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
		auto* capture_ptr = reinterpret_cast<uint8_t*>(buffers[buf.index].start);

		f.data.assign(capture_ptr, capture_ptr + buf.bytesused);

		if (ioctl(fd_dev_, VIDIOC_QBUF, &buf) < 0)
		{
			std::string error_str = std::format("VIDIOC_QBUF failed: {}, {}\n", errno, strerror(errno));
			std::printf(error_str.data());
			continue;
		}

		if (not queue_.push(std::move(f))) 
		{
			continue;
		}

		next_frame += interval;
		// std::this_thread::sleep_until(next_frame);
	}
};

Camera::Camera(LockFreeQueue<Frame, 64>& q, 
				uint32_t fps = 30, 
				uint32_t w = 320, uint32_t h = 240,
				const char* devname = "")
	: queue_(q), target_fps_(fps), width_(w), height_(h)
{
	struct stat st;

	struct v4l2_capability 	   cap;
	struct v4l2_cropcap	   	   cropcap;
	struct v4l2_crop	   	   crop;
	struct v4l2_format	   	   s_fmt;
	struct v4l2_requestbuffers req_buf;

	//open

	if (stat(devname, &st)) 
	{
		std::string error_str = std::format("Cannot inetify '{}': {}, {}\n", devname, errno, strerror(errno));
		throw std::runtime_error(error_str);
	}

	if (not S_ISCHR(st.st_mode)) 
	{
		std::string error_str = std::format("{} is no device\n", devname);
		throw std::runtime_error(error_str);
	}

	fd_dev_ = open(devname, O_RDWR, 0);
	if (fd_dev_ == -1) 
	{
		std::string error_str = std::format("Cannot open '{}': {}, {}\n", devname, errno, strerror(errno));
		throw std::runtime_error(error_str);
	}

	//end open
	
	//S_FMT

	std::memset(&cap, 0, sizeof(cap));
	if (ioctl(fd_dev_, VIDIOC_QUERYCAP, &cap) < 0) 
	{
		std::string error_str = std::format("{} is no V4L2 device\n", devname);
		throw std::runtime_error(error_str);
	}

	if (not (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) 
	{
		std::string error_str = std::format("{} is no video capture device\n", devname);
		throw std::runtime_error(error_str);
	}

	std::memset(&cropcap, 0, sizeof(cropcap));
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (ioctl(fd_dev_, VIDIOC_CROPCAP, &cropcap) == 0) 
	{
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c = cropcap.defrect;

		if (ioctl(fd_dev_, VIDIOC_S_CROP, &crop) < 0) 
		{
			std::printf("Cropping not supported\n");
		}
	};

	std::memset(&s_fmt, 0, sizeof(s_fmt));
	s_fmt.type 				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	s_fmt.fmt.pix.width 		= w;
	s_fmt.fmt.pix.height		= h;
	s_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

	if (ioctl(fd_dev_, VIDIOC_S_FMT, &s_fmt) < 0) 
	{
		throw std::runtime_error("VIDIOC_S_FMT\n");
	}

	//S_FMT end

	//REQBUFS 

	std::memset(&req_buf, 0, sizeof(req_buf));

	req_buf.count 	= 4;
	req_buf.type  	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req_buf.memory 	= V4L2_MEMORY_MMAP;

	if (ioctl(fd_dev_, VIDIOC_REQBUFS, &req_buf) < 0) 
	{
		if (EINVAL == errno) 
		{
			std::string error_str = std::format("{} does not support user pointer i/on\n", devname);
			throw std::runtime_error(error_str);
		} else {
			throw std::runtime_error("VIDIOC_REQBUFS\n");
		}
	}

	//REQBUFS end

	//mmap

	buffers.resize(req_buf.count);
	for (num_buffer = 0; num_buffer < req_buf.count; ++num_buffer) 
	{
		struct v4l2_buffer buf;

		std::memset(&buf, 0, sizeof(buf));
		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= num_buffer;

		if (ioctl(fd_dev_, VIDIOC_QUERYBUF, &buf) < 0) 
		{
			throw std::runtime_error("VIDIOC_QUERYBUF\n");
		}

		buffers[num_buffer].length = buf.length;
		buffers[num_buffer].start =
				mmap(NULL,
					buf.length,
					PROT_READ | PROT_WRITE,
					MAP_SHARED,
					fd_dev_, buf.m.offset);

		if (MAP_FAILED == buffers[num_buffer].start) 
		{
			throw std::runtime_error("mmap\n");
		}

		struct v4l2_buffer q_buf;

		std::memset(&q_buf, 0, sizeof(q_buf));
		q_buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		q_buf.memory = V4L2_MEMORY_MMAP;
		q_buf.index  = num_buffer;

		if (ioctl(fd_dev_, VIDIOC_QBUF, &q_buf) < 0) 
		{
			std::string error_str = std::format("VIDIOC_QBUF: ERROR {}, {}\n", errno, strerror(errno));
			throw std::runtime_error(error_str);
		}
	}
	//mmap end
}

void Camera::start() 
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd_dev_, VIDIOC_STREAMON, &type) < 0) 
	{
		std::string error_str = std::format("VIDIOC_STREAMON: ERROR {}, {}\n", errno, strerror(errno));
		throw std::runtime_error(error_str);
	}
	worker_ = std::jthread([this](std::stop_token st) {
			capture_loop(st);
		});
}

void Camera::stop() 
{
	if (worker_.joinable()) 
	{
		worker_.request_stop();
		worker_.join();
	}
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(fd_dev_, VIDIOC_STREAMOFF, &type) < 0) 
	{			
		std::string error_str = std::format("VIDIOC_STREAMOFF: ERROR {}, {}\n", errno, strerror(errno));
		std::printf(error_str.data());
	}
	for (int i = 0; i < num_buffer; ++i) 
	{
		if (munmap((void*)buffers[i].start, buffers[i].length)) 
		{
			std::printf("munmap\n");
		}
	}
	if (close(fd_dev_) < 0) 
	{
		std::printf("exit\n");
	}
	fd_dev_ = -1;
}

uint32_t Camera::frames_take() const { return frame_seq_; }
uint32_t Camera::frames_bad() const { return frame_error_; }
uint32_t Camera::frames_empty() const { return frame_empty_; }
Camera::~Camera() { stop(); }

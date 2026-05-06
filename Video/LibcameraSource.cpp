#include <LibcameraSource.hpp>
#include <iostream>
#include <format>
#include <fcntl.h>
#include <sys/mman.h>

#include <cerrno>
#include <cstring>

#include <libcamera/libcamera/framebuffer_allocator.h>

LibcameraSource::LibcameraSource(LockFreeQueue<Frame, 64> &queue, uint32_t width, uint32_t height, uint32_t fps)
: queue_(queue), width_(width), height_(height), fps_(fps)
{

}

void LibcameraSource::start()
{
	if(running_.load()) 
	{
		return;
	}

	camera_manager_ = std::make_unique<libcamera::CameraManager>();
	
	if (camera_manager_->start() != 0) 
	{
		throw std::runtime_error("camera_manager_ init error");
	}

	if (camera_manager_->cameras().empty()) 
	{
		throw std::runtime_error("Camera not found");
	}

	for (const auto& camera : camera_manager_->cameras()) 
	{
		std::string message = std::format("Camera found: {}", camera->id());
		std::cout << message << std::endl;
	}
	std::string cam_id = camera_manager_->cameras()[0]->id(); 
	camera_ = camera_manager_->get(cam_id);

	if (camera_ == nullptr) 
	{
		std::string message = std::format("Take camera error: {}", cam_id);
		throw std::runtime_error(message.data());
	}

	if (camera_->acquire() != 0) 
	{
		std::string message = std::format("Take camera error: {}", cam_id);
		throw std::runtime_error(message.data());
	}

	config_ = camera_->generateConfiguration({ libcamera::StreamRole::VideoRecording });

	if (config_.get() == nullptr and config_->empty()) 
	{
		throw std::runtime_error("Config stream");
	}
	auto& stream_config = config_->at(0);
	
	stream_config.size.width = width_;
	stream_config.size.height = height_;

	config_->validate();
	camera_->configure(config_.get());
	stream_ = stream_config.stream();

	std::string message = std::format("Validate stream: {}x{}, format = {}, stride = {}, frameSize = {}", 
	stream_config.size.width, 
	stream_config.size.height, 
	stream_config.pixelFormat.toString(), 
	stream_config.stride, 
	stream_config.frameSize);
	std::cout << message << std::endl;

	allocator_ = std::make_unique<libcamera::FrameBufferAllocator>(camera_);
	allocator_->allocate(stream_);
	
	const auto& buffers = allocator_->buffers(stream_);

	if (buffers.empty()) {
		throw std::runtime_error("Buffers empty");
	} else {
		std::cout << "Allocate: " << buffers.size() << std::endl;
	}

	for (const auto& buffer : buffers) 
	{
		auto& mapped_planes = mapped_buffers_[buffer.get()];

		int fd = buffer->planes()[0].fd.get();
		size_t length = stream_config.frameSize;
		int offset = 0;
		void* address = mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
		if (address == MAP_FAILED) 
		{
			std::cout << "mmap failed: errno=" << errno
			<< " " << std::strerror(errno)
			<< " fd=" << fd
			<< " length=" << length
			<< " offset=" << offset
			<< std::endl;
			throw std::runtime_error("MAP_FAILED");
		}
		
		mapped_planes.push_back({address, length});
		std::cout << "Mapped: " << buffer.get() << " ,planes=" << mapped_planes.size() << std::endl;

		auto request = camera_->createRequest();
		if (not request) 
		{
			throw std::runtime_error("CAMERA REQUEST CREATE FAILED");
		}

		if (request->addBuffer(stream_, buffer.get()) < 0) 
		{
			throw std::runtime_error("ADDED BUFFER FAILED");
		}
		
		requests_.push_back(std::move(request));
	}
	

	running_.store(true);
}

void LibcameraSource::stop()
{
	running_.store(false);

	if (camera_.get()) 
	{
		camera_->stop();
	}

	for (auto& [buffer, planes] : mapped_buffers_)
    {
        for (auto& plane : planes)
        {
            if (plane.address && plane.address != MAP_FAILED && plane.length > 0)
            {
                munmap(plane.address, plane.length);
                plane.address = nullptr;
                plane.length = 0;
            }
        }
    }

    mapped_buffers_.clear();
	requests_.clear();
	allocator_.reset();

	if (camera_.get()) 
	{
		camera_->release();
		camera_.reset();
	}

	if (camera_manager_.get()) 
	{
		camera_manager_->stop();
		camera_manager_.reset();
	}
}

LibcameraSource::~LibcameraSource()
{
	stop();
}

#pragma once

#include "Frame.hpp"
#include <LockFreeQueue.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include <libcamera/libcamera/camera.h>
#include <libcamera/libcamera/camera_manager.h>

struct MappedPlane 
{
	void* address = nullptr;
	size_t length = 0;
};

class LibcameraSource
{
public:
    LibcameraSource(
        LockFreeQueue<Frame, 64>& queue,
        uint32_t width,
        uint32_t height,
        uint32_t fps
    );

    void start();
    void stop();

    LibcameraSource(const LibcameraSource&) = delete;
    LibcameraSource& operator=(const LibcameraSource&) = delete;
    LibcameraSource(LibcameraSource&&) = delete;
    LibcameraSource& operator=(LibcameraSource&&) = delete;

    ~LibcameraSource();

private:
    LockFreeQueue<Frame, 64>& queue_;

	libcamera::Stream *stream_ = nullptr;
	std::unique_ptr<libcamera::FrameBufferAllocator> allocator_;
	std::unique_ptr<libcamera::CameraConfiguration> config_;
	std::unique_ptr<libcamera::CameraManager> camera_manager_;
	std::shared_ptr<libcamera::Camera> camera_;
	
	std::vector<std::unique_ptr<libcamera::Request>> requests_;
	std::unordered_map<const libcamera::FrameBuffer*, std::vector<MappedPlane>> mapped_buffers_;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t fps_ = 0;

    std::atomic_bool running_{false};
};
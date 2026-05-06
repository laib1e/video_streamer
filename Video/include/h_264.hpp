#pragma once
#include "Frame.hpp"

#include <span>
#include <cstdint>
#include <stdexcept>
#include <format>
#include <concepts>
#include <functional>
#include <memory>

extern "C" 
{
	#include <libavcodec/avcodec.h>
	#include <libavutil/opt.h>
	#include <libavutil/imgutils.h>
	#include <libswscale/swscale.h>
}

struct AVContextFree 
{
	void operator()(AVCodecContext* context_p) noexcept
	{
		if (context_p) 
		{
			avcodec_free_context(&context_p);
		}
	}
};

struct AVFrameFree 
{
	void operator()(AVFrame* frame_p) noexcept
	{
		if (frame_p) 
		{
			av_frame_free(&frame_p);
		}
	}
};

struct AVPacketFree 
{
	void operator()(AVPacket* packet_p) noexcept 
	{
		if (packet_p) 
		{
			av_packet_free(&packet_p);
		}
	}
};

struct SwsContextFree 
{
	void operator()(SwsContext* sws_context_p) noexcept
	{
		if (sws_context_p) 
		{
			sws_freeContext(sws_context_p);
		}
	}
};

using EncodedDataCallback = std::function<bool(std::span<const uint8_t>)>;

class H264Encoder
{
public:
	void init(uint32_t width, uint32_t height, uint32_t fps, uint32_t bitrate);
	bool encode(const Frame& frame, const EncodedDataCallback& sink);
	void close();

	H264Encoder() = default;
	~H264Encoder() = default;
    H264Encoder(const H264Encoder&) = delete;
    H264Encoder& operator=(const H264Encoder&) = delete;

private:
	const AVCodec* codec_ = nullptr;
	std::unique_ptr<AVCodecContext, AVContextFree> codec_ctx_;
	std::unique_ptr<AVFrame, AVFrameFree> yuv_frame_;
	std::unique_ptr<AVPacket, AVPacketFree> packet_;
	std::unique_ptr<SwsContext, SwsContextFree> sws_ctx_;

	uint32_t width_   = 0;
	uint32_t height_  = 0;
	uint32_t fps_	  = 0;
	uint32_t bitrate_ = 0;

	int64_t pts_ 	  = 0;
};
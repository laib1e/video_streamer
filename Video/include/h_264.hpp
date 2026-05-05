#pragma once
#include "Frame.hpp"

#include <span>
#include <cstdint>
#include <stdexcept>
#include <format>
#include <concepts>
#include <functional>


extern "C" 
{
	#include <libavcodec/avcodec.h>
	#include <libavutil/opt.h>
	#include <libavutil/imgutils.h>
	#include <libswscale/swscale.h>
}

using EncodedDataCallback = std::function<bool(std::span<const uint8_t>)>;

class H264Encoder
{
public:
	void init(uint32_t width, uint32_t height, uint32_t fps, uint32_t bitrate);

	bool encode(const Frame& frame, const EncodedDataCallback& sink);
	
	void close();

	H264Encoder() = default;
    H264Encoder(const H264Encoder&) = delete;
    H264Encoder& operator=(const H264Encoder&) = delete;
	~H264Encoder();

private:
	const AVCodec* codec_ = nullptr;
	AVCodecContext* codec_ctx_ = nullptr;
	AVFrame* yuv_frame_ = nullptr;
	AVPacket* packet_ = nullptr;
	SwsContext* sws_ctx_ = nullptr;

	uint32_t width_   = 0;
	uint32_t height_  = 0;
	uint32_t fps_	  = 0;
	uint32_t bitrate_ = 0;

	int64_t pts_ 	  = 0;
};
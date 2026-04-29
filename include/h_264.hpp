#pragma once
#include "Frame.hpp"

#include <span>
#include <cstdint>
#include <stdexcept>
#include <format>
#include <concepts>

extern "C" 
{
	#include <libavcodec/avcodec.h>
	#include <libavutil/opt.h>
	#include <libavutil/imgutils.h>
	#include <libswscale/swscale.h>
}

template<typename Sink>
concept EncodedDataSink = requires(Sink sink, std::span<const uint8_t> data)
{
    { sink(data) } -> std::convertible_to<bool>;
};

class H264Encoder
{
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

public:

	void init(uint32_t width, uint32_t height, uint32_t fps, uint32_t bitrate)
	{
		width_ = width;
		height_ = height;
		bitrate_ = bitrate;
		fps_ = fps;

		codec_ = avcodec_find_encoder_by_name("libx264");
		if (not codec_)
			codec_ = avcodec_find_encoder(AV_CODEC_ID_H264);
		if (not codec_)
			throw std::runtime_error("H.264 encoder not found");

		codec_ctx_ = avcodec_alloc_context3(codec_);
		if (not codec_ctx_)
			throw std::runtime_error("Could not allocate codec context");

		codec_ctx_->width = width_;
		codec_ctx_->height = height_;
		codec_ctx_->bit_rate = bitrate_;

		codec_ctx_->time_base = AVRational{1, static_cast<int>(fps_)};
		codec_ctx_->framerate = AVRational{static_cast<int>(fps_), 1};

		codec_ctx_->gop_size = static_cast<int>(fps_);
		codec_ctx_->max_b_frames = 0;
		codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;

		if (std::string_view(codec_->name) == "libx264") 
		{
			av_opt_set(codec_ctx_->priv_data, "preset", "ultrafast", 0);
			av_opt_set(codec_ctx_->priv_data, "tune", "zerolatency", 0);
			av_opt_set(codec_ctx_->priv_data, "x264-params", "bframes=0:rc-lookahead=0:sync-lookahead=0:repeat-headers=1", 0);
		}

		int ret = avcodec_open2(codec_ctx_, codec_, nullptr);
		if (ret < 0) 
		{
			std::string message = std::format("Could not open H.264 codec, error: {}", av_err2str(ret));
			throw std::runtime_error(message.data());
		}

		yuv_frame_ = av_frame_alloc();
		if (not yuv_frame_) 
			throw std::runtime_error("Could not allocate frame");

		yuv_frame_->format = codec_ctx_->pix_fmt;
		yuv_frame_->width  = codec_ctx_->width;
		yuv_frame_->height = codec_ctx_->height;

		ret = av_frame_get_buffer(yuv_frame_, 32);
		if (ret < 0) 
		{
			std::string message = std::format("Could not allocate buffer, error: {}", av_err2str(ret));
			throw std::runtime_error(message.data());
		}

		packet_ = av_packet_alloc();
		if (not packet_)
			throw std::runtime_error("Could not allocate packet");
		
		sws_ctx_ = sws_getContext(
			width_,
			height_,
			AV_PIX_FMT_YUYV422,
			width_,
			height_,
			AV_PIX_FMT_YUV420P,
			SWS_FAST_BILINEAR,
			nullptr,
			nullptr,
			nullptr
		);
		if (not sws_ctx_)
			throw std::runtime_error("Could not create sws context");

	}
	
	template<EncodedDataSink Sink>
	bool encode(const Frame& frame, Sink&& sink)
	{
		const uint8_t* src_data[4] = { frame.data.data(), nullptr, nullptr, nullptr };
		int src_linesize[4]		   = { static_cast<int>(width_ * 2), 0, 0, 0 };

		int ret = av_frame_make_writable(yuv_frame_);
		if (ret < 0) 
		{
			std::string message = std::format("Frame is not writable, error: {}", av_err2str(ret));
			throw std::runtime_error(message.data());
		}

		ret = sws_scale(
			sws_ctx_, 
			src_data, 
			src_linesize, 
			0, 
			static_cast<int>(height_), 
			yuv_frame_->data, 
			yuv_frame_->linesize
		);
		if (ret != static_cast<int>(height_))
			throw std::runtime_error("sws_scale failed");

		yuv_frame_->pts = pts_++;
		codec_ctx_->time_base = AVRational{1, static_cast<int>(fps_)};

		ret = avcodec_send_frame(codec_ctx_, yuv_frame_);
		if (ret < 0) 
		{
			std::string message = std::format("Could not send frame to encoder, error: {}", av_err2str(ret));
			throw std::runtime_error(message.data());
		}

		while (ret >= 0) 
		{
			ret = avcodec_receive_packet(codec_ctx_, packet_);

			if (ret == AVERROR(EAGAIN) or ret == AVERROR_EOF)
				break;

			if (ret < 0) 
			{
				std::string message = std::format("Could not receive packet from encoder, error: {}", av_err2str(ret));
				throw std::runtime_error(message.data());
			}

			std::span<const uint8_t> encoded_data 
			{
				packet_->data,
				static_cast<size_t>(packet_->size)
			};

			if (not sink(encoded_data)) 
			{
				av_packet_unref(packet_);
				return false;
			}

			av_packet_unref(packet_);
		}
		return true;
	}

	void close() 
	{
		if (sws_ctx_)
			sws_freeContext(sws_ctx_);
		
		if (packet_)
			av_packet_free(&packet_);
		
		if (yuv_frame_) 
			av_frame_free(&yuv_frame_);

		if (codec_ctx_)
			avcodec_free_context(&codec_ctx_);
	}

	H264Encoder() = default;
    H264Encoder(const H264Encoder&) = delete;
    H264Encoder& operator=(const H264Encoder&) = delete;
	~H264Encoder() 
	{
		close();
	}
};
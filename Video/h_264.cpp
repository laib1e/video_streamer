#include "h_264.hpp"

void H264Encoder::init(uint32_t width, uint32_t height, uint32_t fps, uint32_t bitrate)
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

	codec_ctx_.reset(avcodec_alloc_context3(codec_));
	if (not codec_ctx_.get())
		throw std::runtime_error("Could not allocate codec context");

	codec_ctx_.get()->width = width_;
	codec_ctx_.get()->height = height_;
	codec_ctx_.get()->bit_rate = bitrate_;

	codec_ctx_.get()->time_base = AVRational{1, static_cast<int>(fps_)};
	codec_ctx_.get()->framerate = AVRational{static_cast<int>(fps_), 1};

	codec_ctx_.get()->gop_size = static_cast<int>(fps_);
	codec_ctx_.get()->max_b_frames = 0;
	codec_ctx_.get()->pix_fmt = AV_PIX_FMT_YUV420P;

	if (std::string_view(codec_->name) == "libx264") 
	{
		av_opt_set(codec_ctx_.get()->priv_data, "preset", "ultrafast", 0);
		av_opt_set(codec_ctx_.get()->priv_data, "tune", "zerolatency", 0);
		av_opt_set(codec_ctx_.get()->priv_data, "x264-params", "bframes=0:rc-lookahead=0:sync-lookahead=0:repeat-headers=1", 0);
	}

	int ret = avcodec_open2(codec_ctx_.get(), codec_, nullptr);
	if (ret < 0) 
	{
		std::string message = std::format("Could not open H.264 codec, error: {}", av_err2str(ret));
		throw std::runtime_error(message.data());
	}

	yuv_frame_.reset(av_frame_alloc());
	if (not yuv_frame_.get()) 
		throw std::runtime_error("Could not allocate frame");

	yuv_frame_.get()->format = codec_ctx_.get()->pix_fmt;
	yuv_frame_.get()->width  = codec_ctx_.get()->width;
	yuv_frame_.get()->height = codec_ctx_.get()->height;

	ret = av_frame_get_buffer(yuv_frame_.get(), 32);
	if (ret < 0) 
	{
		std::string message = std::format("Could not allocate buffer, error: {}", av_err2str(ret));
		throw std::runtime_error(message.data());
	}

	packet_.reset(av_packet_alloc());
	if (not packet_.get())
		throw std::runtime_error("Could not allocate packet");
	
	sws_ctx_.reset(sws_getContext(
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
	));

	if (not sws_ctx_.get())
		throw std::runtime_error("Could not create sws context");

}

bool H264Encoder::encode(const Frame& frame, const EncodedDataCallback& sink)
{
	const uint8_t* src_data[4] = { frame.data.data(), nullptr, nullptr, nullptr };
	int src_linesize[4]		   = { static_cast<int>(width_ * 2), 0, 0, 0 };

	int ret = av_frame_make_writable(yuv_frame_.get());
	if (ret < 0) 
	{
		std::string message = std::format("Frame is not writable, error: {}", av_err2str(ret));
		throw std::runtime_error(message.data());
	}

	ret = sws_scale(
		sws_ctx_.get(), 
		src_data, 
		src_linesize, 
		0, 
		static_cast<int>(height_), 
		yuv_frame_.get()->data, 
		yuv_frame_.get()->linesize
	);
	if (ret != static_cast<int>(height_))
		throw std::runtime_error("sws_scale failed");

	yuv_frame_.get()->pts = pts_++;
	codec_ctx_.get()->time_base = AVRational{1, static_cast<int>(fps_)};

	ret = avcodec_send_frame(codec_ctx_.get(), yuv_frame_.get());
	if (ret < 0) 
	{
		std::string message = std::format("Could not send frame to encoder, error: {}", av_err2str(ret));
		throw std::runtime_error(message.data());
	}

	while (ret >= 0) 
	{
		ret = avcodec_receive_packet(codec_ctx_.get(), packet_.get());

		if (ret == AVERROR(EAGAIN) or ret == AVERROR_EOF)
			break;

		if (ret < 0) 
		{
			std::string message = std::format("Could not receive packet from encoder, error: {}", av_err2str(ret));
			throw std::runtime_error(message.data());
		}

		std::span<const uint8_t> encoded_data 
		{
			packet_.get()->data,
			static_cast<size_t>(packet_.get()->size)
		};

		if (not sink(encoded_data)) 
		{
			av_packet_unref(packet_.get());
			return false;
		}

		av_packet_unref(packet_.get());
	}
	return true;
}

void H264Encoder::close() 
{
	sws_ctx_.reset();
	packet_.reset();
	yuv_frame_.reset();
	codec_ctx_.reset();
}
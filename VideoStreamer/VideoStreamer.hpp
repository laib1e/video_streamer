#pragma once
#include <LockFreeQueue.hpp>
#include <h_264.hpp>
#include <thread>
#include <concepts>
#include <span>
#include <chrono>
#include <functional>
#include <cstdio>

template<typename T>
concept TransportPolicy = requires(T t, std::span<const uint8_t> data) 
{
    { t.send(data) } -> std::convertible_to<bool>;
    { t.open(std::declval<const char*>(), uint16_t{}) } -> std::convertible_to<bool>;
    { t.close() };
};

template<typename T>
concept EncoderPolicy = requires(
    T encoder,
    const Frame& frame,
    uint32_t width,
    uint32_t height,
    uint32_t fps,
    uint32_t bitrate
)
{
    { encoder.init(width, height, fps, bitrate) };
    { encoder.close() };
};

template<TransportPolicy Transport, EncoderPolicy Encoder>
class VideoStreamer 
{
public:
    explicit VideoStreamer(LockFreeQueue<Frame, 64>& q, uint32_t width, uint32_t height, uint32_t fps, uint32_t bitrate);
    
	bool set_options(const char* ip, uint16_t port);
    bool start();
    void stop();

    uint32_t frames_sent();

    ~VideoStreamer();

private:
    Encoder encoder_;
    Transport transport_;
    LockFreeQueue<Frame, 64>& queue_;
    std::jthread worker_;
    uint32_t frames_sent_ = 0;

	uint32_t width_   = 0;
	uint32_t height_  = 0;
	uint32_t fps_	  = 0;
	uint32_t bitrate_ = 0;

    void run(std::stop_token st);
};

template<TransportPolicy Transport, EncoderPolicy Encoder>
void VideoStreamer<Transport, Encoder>::run(std::stop_token st) 
{
	int spin_count = 0;
	while (!st.stop_requested()) 
	{
		Frame frame;
		if (queue_.pop(frame)) 
		{
			auto encoder_sink = [this](std::span<const uint8_t> packet) -> bool {
				return transport_.send(packet);
			};
			encoder_.encode(frame, encoder_sink);
			frames_sent_++;
			spin_count = 0;
		} else {
			// Adaptive backoff
			if (spin_count < 100) {
				spin_count++;
			} else if (spin_count < 1000) {
				std::this_thread::yield();
				spin_count++;
			} else {
				std::this_thread::sleep_for(std::chrono::microseconds(100));
			}
		}
	}
}

template<TransportPolicy Transport, EncoderPolicy Encoder>
VideoStreamer<Transport, Encoder>::VideoStreamer(LockFreeQueue<Frame, 64>& q,
	uint32_t width, uint32_t height, uint32_t fps, uint32_t bitrate) : queue_(q), width_(width), height_(height), fps_(fps), bitrate_(bitrate) {}


template<TransportPolicy Transport, EncoderPolicy Encoder>
bool VideoStreamer<Transport, Encoder>::set_options(const char* ip, uint16_t port) 
{
	return transport_.open(ip, port);
}

template<TransportPolicy Transport, EncoderPolicy Encoder>
bool VideoStreamer<Transport, Encoder>::start() 
{
	try {
		encoder_.init(width_, height_, fps_, bitrate_);
	} catch (const std::exception& e) {
		std::string message = std::format("Encoder init failed: {}\n", e.what());
		std::printf(message.data());
		return false;
	}
	worker_ = std::jthread([this](std::stop_token st) { run(st); });
	return true;
}

template <TransportPolicy Transport, EncoderPolicy Encoder>
uint32_t VideoStreamer<Transport, Encoder>::frames_sent()
{
	return frames_sent_;
}

template<TransportPolicy Transport, EncoderPolicy Encoder>
void VideoStreamer<Transport, Encoder>::stop() 
{
	if (worker_.joinable()) 
	{
		worker_.request_stop();
		worker_.join();
	}
	encoder_.close();
	transport_.close();
}

template <TransportPolicy Transport, EncoderPolicy Encoder>
VideoStreamer<Transport, Encoder>::~VideoStreamer()
{
	stop();
}
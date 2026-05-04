#include <VideoStreamer.hpp>

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
VideoStreamer<Transport, Encoder>::VideoStreamer(LockFreeQueue<Frame, 64>& q) : queue_(q) {}

template<TransportPolicy Transport, EncoderPolicy Encoder>
bool VideoStreamer<Transport, Encoder>::start(const char* ip, uint16_t port, uint32_t width, uint32_t height, uint32_t fps, uint32_t bitrate) 
{
	if (!transport_.open(ip, port)) return false;
	try {
		encoder_.init(width, height, fps, bitrate);
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
	transport_.close();
}

template <TransportPolicy Transport, EncoderPolicy Encoder>
VideoStreamer<Transport, Encoder>::~VideoStreamer()
{
	stop();
}

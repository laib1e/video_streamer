#pragma once
#include "lock_free_queue.hpp"
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

template<TransportPolicy Transport>
class VideoStreamer 
{
    Transport transport_;
    LockFreeQueue<Frame, 64>& queue_;
    std::jthread worker_;
    uint32_t frames_sent_ = 0;

    void run(std::stop_token st) 
	{
        int spin_count = 0;
        while (!st.stop_requested()) 
		{
            Frame frame;
            if (queue_.pop(frame)) 
			{
                transport_.send(std::span<const uint8_t>(frame.data.data(), frame.data.size()));
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

public:
    explicit VideoStreamer(LockFreeQueue<Frame, 64>& q) : queue_(q) {}

    bool start(const char* ip, uint16_t port) 
	{
        if (!transport_.open(ip, port)) return false;
        worker_ = std::jthread([this](std::stop_token st) { run(st); });
        return true;
    }

    void stop() 
	{
        if (worker_.joinable()) 
		{
            worker_.request_stop();
            worker_.join();
        }
        transport_.close();
    }

    uint32_t frames_sent() const { return frames_sent_; }

    ~VideoStreamer() 
	{
        stop();
    }
};

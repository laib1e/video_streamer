#pragma once
#include "LockFreeQueue.hpp"
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
    explicit VideoStreamer(LockFreeQueue<Frame, 64>& q);
    
    bool start(const char* ip, uint16_t port, uint32_t width, uint32_t height, uint32_t fps, uint32_t bitrate);
    uint32_t frames_sent();
    void stop();

    ~VideoStreamer();

private:
    Encoder encoder_;
    Transport transport_;
    LockFreeQueue<Frame, 64>& queue_;
    std::jthread worker_;
    uint32_t frames_sent_ = 0;

    void run(std::stop_token st);
};

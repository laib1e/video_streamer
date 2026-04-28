#include "lock_free_queue.hpp"
#include "camera_sources.hpp"
#include "video_streamer.hpp"
#include "rtp_udp_transport.hpp"

#include <cstdio>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

static std::atomic<bool> running{true};

void signal_handler(int) {
    running.store(false, std::memory_order_relaxed);
}

int main(int argc, char* argv[]) {
    const char* dest_ip = "192.168.0.253";
    uint16_t dest_port = 9000;
    uint32_t fps = 30;
    int duration_sec = 600;

    LockFreeQueue<Frame, 64> queue;
    CameraSource camera(queue, fps, 320, 240, "/dev/video0");
    VideoStreamer<RtpUdpTransport> streamer(queue);

    if (!streamer.start(dest_ip, dest_port)) 
	{
        std::fprintf(stderr, "Failed to start streamer\n");
        return 1;
    }
	std::string start_message = std::format("[OK] Streamer started (RtpUdpTransport -> {}:{})\n", dest_ip, dest_port);
    std::printf(start_message.data());

    camera.start();

    std::signal(SIGINT, signal_handler);
    auto start = std::chrono::steady_clock::now();
    while (running.load(std::memory_order_relaxed)) 
	{
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start).count();
		std::string message = std::format("  [{}s] camera: {} frames, streamer: {} sent\n", elapsed, camera.frames_take(), streamer.frames_sent());
		std::printf(message.data());

        if (elapsed >= duration_sec) break;
    }

    std::printf("\n[..] Stopping...\n");
    camera.stop();
    streamer.stop();

	
	std::string frames_take = std::format("[OK] Camera generated: {} frames\n", camera.frames_take());
	std::string frames_sent = std::format("[OK] Streamer sent:    {} frames\n", streamer.frames_sent());
    std::printf(frames_take.data());
    std::printf(frames_sent.data());
    std::printf("[OK] Done.\n");

    return 0;
}

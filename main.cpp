#include <LockFreeQueue.hpp>
#include <Camera.hpp>
#include <RTPServer.hpp>
#include <h_264.hpp>
#include <RTSPServer.hpp>
#include <VideoStreamer.hpp>

#include <cstdio>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>

using H264RTPVideoStream = VideoStreamer<RTPServer, H264Encoder>;

int main(int argc, char* argv[]) {
    const char* dest_ip = "192.168.0.253";
    uint16_t dest_port = 9000;
    uint32_t fps = 30;
    int duration_sec = 600;

    LockFreeQueue<Frame, 64> queue;
    Camera camera(queue, fps, 640, 480, "/dev/video14");
    H264RTPVideoStream streamer(queue, 640, 480, fps, 600000);
    RTSPServer<H264RTPVideoStream> server(5983, streamer);
    try {
        camera.start();
    } catch (const std::exception& e) {
		std::string message = std::format("Camera start failed: {}\n", e.what());
		std::printf(message.data());
        return -1;
    }

    while(true) 
    {
        server.run();
    }

    return 0;
}

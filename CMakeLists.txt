cmake_minimum_required(VERSION 3.20)
project(video_streamer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(LockFreeQueue)
add_subdirectory(RTP)
add_subdirectory(RTSP)
add_subdirectory(Video)
add_subdirectory(VideoStreamer)

file(GLOB SOURCES "main.cpp")
add_executable(streamer ${SOURCES})
target_link_libraries(Video PRIVATE LockFreeQueue)

target_link_libraries(streamer PRIVATE LockFreeQueue VideoStreamer RTSP RTP Video pthread ${FFMPEG_LIBRARIES})
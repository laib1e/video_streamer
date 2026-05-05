cmake_minimum_required(VERSION 3.20)
project(video_streamer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED 
	libavcodec
	libavformat
	libavutil
	libswscale
)

add_subdirectory(LockFreeQueue)
add_subdirectory(Option)
add_subdirectory(RTP)
add_subdirectory(RTSP)
add_subdirectory(Video)

file(GLOB SOURCES "src/*.cpp")
add_executable(streamer ${SOURCES})
target_include_directories(streamer PUBLIC include ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(Video PRIVATE Queue)
target_link_libraries(RTSP PRIVATE Queue)

target_link_libraries(streamer PRIVATE Queue RTSP Video pthread ${FFMPEG_LIBRARIES})
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

add_executable(streamer src/main.cpp)
target_include_directories(streamer PRIVATE include ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(streamer PRIVATE pthread ${FFMPEG_LIBRARIES})
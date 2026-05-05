cmake_minimum_required(VERSION 3.20)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED 
	libavcodec
	libavformat
	libavutil
	libswscale
)

file(GLOB SOURCES "*.cpp")
add_library(Video STATIC ${SOURCES})
target_include_directories(Video PUBLIC ${FFMPEG_INCLUDE_DIRS} include)
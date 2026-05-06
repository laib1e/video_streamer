cmake_minimum_required(VERSION 3.20)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED 
	libavcodec
	libavformat
	libavutil
	libswscale
)

pkg_check_modules(LIBCAMERA REQUIRED libcamera)

file(GLOB SOURCES "*.cpp")
add_library(Video STATIC ${SOURCES})
target_include_directories(Video PUBLIC ${FFMPEG_INCLUDE_DIRS} ${LIBCAMERA_INCLUDE_DIRS} include)
target_link_libraries(Video PRIVATE ${FFMPEG_LIBRARIES} ${LIBCAMERA_LIBRARIES})
target_compile_options(Video PRIVATE ${LIBCAMERA_CFLAGS_OTHER})
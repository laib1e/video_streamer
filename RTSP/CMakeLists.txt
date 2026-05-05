cmake_minimum_required(VERSION 3.20)

add_library(RTSP INTERFACE)
target_include_directories(RTSP INTERFACE include ${CMAKE_CURRENT_SOURCE_DIR})
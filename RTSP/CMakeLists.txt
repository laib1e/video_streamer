cmake_minimum_required(VERSION 3.20)

add_library(RTSP STATIC RTSPServer.cpp)
target_include_directories(RTSP PUBLIC include)
cmake_minimum_required(VERSION 3.20)

add_library(RTP STATIC RTPServer.cpp)
target_include_directories(RTP PUBLIC include)
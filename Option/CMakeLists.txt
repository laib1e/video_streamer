cmake_minimum_required(VERSION 3.20)

add_library(FileTransport STATIC FileTransport.cpp)
target_include_directories(FileTransport PUBLIC include)
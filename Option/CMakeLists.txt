cmake_minimum_required(VERSION 3.20)

add_library(OPTION STATIC FileTransport.cpp)
target_include_directories(OPTION PUBLIC include)
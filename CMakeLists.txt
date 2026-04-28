cmake_minimum_required(VERSION 3.20)
project(video_streamer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(streamer src/main.cpp)
target_include_directories(streamer PRIVATE include)
target_link_libraries(streamer PRIVATE pthread)
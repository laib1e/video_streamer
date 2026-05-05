cmake_minimum_required(VERSION 3.20)

add_library(Queue STATIC LockFreeQueue.cpp)
target_include_directories(Queue PUBLIC include)
cmake_minimum_required(VERSION 3.20)

add_library(LockFreeQueue INTERFACE)
target_include_directories(LockFreeQueue INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
cmake_minimum_required(VERSION 3.20)

file(GLOB SOURCES "*.cpp")
add_library(Video STATIC ${SOURCES})
target_include_directories(Video PUBLIC include)
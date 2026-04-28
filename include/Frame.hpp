#pragma once
#include <vector>
#include <cstdint>

struct Frame 
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t sequence = 0;
    uint64_t timestamp_us = 0;
    std::vector<uint8_t> data;
};
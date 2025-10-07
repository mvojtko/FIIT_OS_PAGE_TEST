#pragma once

#include <cstring>

#include "gtest/gtest.h"

extern "C" {
#include "ram.h"
}

class RamTestBase : public ::testing::Test
{
  protected:
    static constexpr uint16_t RAM_SIZE = 1024 * 16;  // 16KB of simulated RAM
    static constexpr uint8_t PAGE_SIZE = 128;        // each page is 128B
    static constexpr uint16_t NUM_FRAMES = RAM_SIZE / PAGE_SIZE;

    std::vector<uint8_t> ram;

    RamTestBase()
    {
        ram.resize(RAM_SIZE);
        std::memset(ram.data(), 0, ram.size());
        init_ram(ram.data(), ram.size(), PAGE_SIZE);
    }

    ~RamTestBase()
    {
        destroy_ram();
    }

    uint16_t getOccupiedFrames(bool print, bool *isContinuous);
};

uint16_t nextPow2(uint16_t num);

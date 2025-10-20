#pragma once

#include <cstring>

#include "gtest/gtest.h"

extern "C" {
#include "ram.h"
}

class RamTestBase : public ::testing::Test
{
  protected:
    static constexpr uint16_t RAM_SIZE = 1024 * 2;   // 2KB of simulated RAM
    static constexpr uint8_t PAGE_SIZE = 128;        // each page is 128B
    static constexpr uint16_t NUM_FRAMES = RAM_SIZE / PAGE_SIZE;

    uint8_t ram[RAM_SIZE];

    RamTestBase()
    {
        std::memset(ram, 0, RAM_SIZE);
        init_ram(ram, RAM_SIZE, PAGE_SIZE);
    }

    ~RamTestBase()
    {
        destroy_ram();
    }

    uint16_t getOccupiedFrames(bool *isContinuous);
    void dumpRamState();
};

uint16_t nextPow2(uint16_t num);

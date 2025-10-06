#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <vector>

extern "C" {
#include "mmu.h"
#include "ram.h"
}

class MMUTest : public ::testing::Test
{
  protected:
    static constexpr uint16_t RAM_SIZE = 1024;  // 1KB of simulated RAM
    static constexpr uint8_t PAGE_SIZE = 64;    // each page is 64B
    static constexpr uint16_t NUM_FRAMES = RAM_SIZE / PAGE_SIZE;
    static constexpr uint16_t NUM_PAGES = 0xFFFF;
    static constexpr uint16_t OFFSET_MASK = PAGE_SIZE - 1;

    std::vector<uint8_t> ram;
    std::vector<tPageTableEntry> table;

    void SetUp() override
    {
        ram.resize(RAM_SIZE);
        table.resize(NUM_PAGES);
        std::memset(ram.data(), 0, ram.size());
        std::memset(table.data(), 0, table.size());
        init_ram(ram.data(), ram.size(), PAGE_SIZE);
        set_page_table(table.data());
    }

    void TearDown() override
    {
        destroy_ram();
        set_page_table(nullptr);
    }

    void SetTableEntry(uint16_t id, tPageTableEntry &&entry)
    {
        table[id] = std::move(entry);
    }
};

// get_physical_address tests

TEST_F(MMUTest, GetPhysicalAddressFailsWithNullPointer)
{
    set_page_table(nullptr);
    int ret = get_physical_address(0x1234, nullptr);
    EXPECT_EQ(ret, -3);
}

TEST_F(MMUTest, GetPhysicalAddressFailsWithoutPageTable)
{
    uint16_t phys = 0;
    set_page_table(nullptr);
    int ret = get_physical_address(0x1234, &phys);
    EXPECT_EQ(ret, -4);
    EXPECT_EQ(phys, 0);
}

TEST_F(MMUTest, GetPhysicalAddressFailsWithoutRamInit)
{
    uint16_t phys = 0;
    destroy_ram();
    int ret = get_physical_address(0x1234, &phys);
    EXPECT_EQ(ret, -5);
    EXPECT_EQ(phys, 0);
}

TEST_F(MMUTest, GetPhysicalAddressSegFaultOnRWX000)
{
    uint16_t phys = 0;
    int ret = get_physical_address(0x1234, &phys);
    EXPECT_EQ(ret, -2);
    EXPECT_EQ(phys, 0);
}

TEST_F(MMUTest, GetPhysicalAddressPageFaultOnP0)
{
    uint16_t phys = 0;
    SetTableEntry(0x1234 / PAGE_SIZE, {.r = 0x1});

    int ret = get_physical_address(0x1234, &phys);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(phys, 0);
}

TEST_F(MMUTest, GetPhysicalAddressSuccessOnP1FrameId0)
{
    uint16_t phys = 0;
    uint16_t exp_phys = 0x1234 & OFFSET_MASK;
    SetTableEntry(0x1234 / PAGE_SIZE, {.r = 0x1, .p_bit = 0x1});

    int ret = get_physical_address(0x1234, &phys);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(phys, exp_phys);
}

TEST_F(MMUTest, GetPhysicalAddressSuccessOnP1FrameIdMAX)
{
    uint16_t phys = 0;
    uint16_t exp_phys = (NUM_FRAMES - 1) * PAGE_SIZE + (0x1234 & OFFSET_MASK);
    SetTableEntry(0x1234 / PAGE_SIZE, {.r = 0x1, .p_bit = 0x1, .frame_id = NUM_FRAMES - 1});

    int ret = get_physical_address(0x1234, &phys);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(phys, exp_phys);
}

TEST_F(MMUTest, GetPhysicalAddressSuccessOnP1FrameId)
{
    uint16_t phys = 0;
    uint16_t exp_phys = 55 * PAGE_SIZE + (0x1234 & OFFSET_MASK);
    SetTableEntry(0x1234 / PAGE_SIZE, {.r = 0x1, .p_bit = 0x1, .frame_id = 55});

    int ret = get_physical_address(0x1234, &phys);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(phys, exp_phys);
}

// fetch_instruction tests

TEST_F(MMUTest, FetchInstructionWithoutPageTable)
{
    uint8_t instr = 0;
    set_page_table(nullptr);
    int ret = fetch_instruction(0x2000, &instr);
    EXPECT_EQ(ret, -4);
    EXPECT_EQ(instr, 0);
}

TEST_F(MMUTest, FetchInstructionFailsWithoutRamInit)
{
    uint8_t instr = 0;
    destroy_ram();
    int ret = fetch_instruction(0x2000, &instr);
    EXPECT_EQ(ret, -5);
    EXPECT_EQ(instr, 0);
}

TEST_F(MMUTest, FetchInstructionSegFaultOnRWX000)
{
    uint8_t instr = 0;
    int ret = fetch_instruction(0x1234, &instr);
    EXPECT_EQ(ret, -2);
    EXPECT_EQ(instr, 0);
}

TEST_F(MMUTest, FetchInstructionPageFaultOnP0)
{
    uint8_t instr = 0;
    SetTableEntry(0x1234 / PAGE_SIZE, {.x = 0x1});

    int ret = fetch_instruction(0x1234, &instr);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(instr, 0);
}

TEST_F(MMUTest, FetchInstructionSuccessOnP1FrameId0)
{
    uint8_t instr = 0;
    ram[0x1234 & OFFSET_MASK] = 0xcd;
    SetTableEntry(0x1234 / PAGE_SIZE, {.x = 0x1, .p_bit = 0x1});

    int ret = fetch_instruction(0x1234, &instr);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(instr, 0xcd);
}

TEST_F(MMUTest, FetchInstructionSuccessOnP1FrameIdMAX)
{
    uint8_t instr = 0;
    ram[(NUM_FRAMES - 1) * PAGE_SIZE + (0x1234 & OFFSET_MASK)] = 0xcd;
    SetTableEntry(0x1234 / PAGE_SIZE, {.x = 0x1, .p_bit = 0x1, .frame_id = NUM_FRAMES - 1});

    int ret = fetch_instruction(0x1234, &instr);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(instr, 0xcd);
}

TEST_F(MMUTest, FetchInstructionSuccessOnP1FrameId)
{
    uint8_t instr = 0;
    ram[55 * PAGE_SIZE + (0x1234 & OFFSET_MASK)] = 0xcd;
    SetTableEntry(0x1234 / PAGE_SIZE, {.x = 0x1, .p_bit = 0x1, .frame_id = 55});

    int ret = fetch_instruction(0x1234, &instr);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(instr, 0xcd);
}

// load_data tests

TEST_F(MMUTest, LoadDataWithoutPageTable)
{
    uint8_t data = 0;
    set_page_table(nullptr);
    int ret = load_data(0x3000, &data);
    EXPECT_EQ(ret, -4);
    EXPECT_EQ(data, 0);
}

TEST_F(MMUTest, LoadDataFailsWithoutRamInit)
{
    uint8_t data = 0;
    destroy_ram();
    int ret = load_data(0x2000, &data);
    EXPECT_EQ(ret, -5);
    EXPECT_EQ(data, 0);
}

// store_data tests

TEST_F(MMUTest, StoreDataWithoutPageTable)
{
    set_page_table(nullptr);
    int ret = store_data(0x4000, 0xAB);
    EXPECT_EQ(ret, -4);
}

TEST_F(MMUTest, StoreDataFailsWithoutRamInit)
{
    destroy_ram();
    int ret = store_data(0x2000, 0xAB);
    EXPECT_EQ(ret, -5);
}

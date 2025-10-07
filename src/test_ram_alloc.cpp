#include <cstring>

#include "gtest/gtest.h"
#include "test_ram.h"

extern "C" {
#include "ram.h"
}

#define CHECK_BOUNDARIES(ptr)                \
    ASSERT_NE((ptr), nullptr);               \
    EXPECT_GE((uint8_t *)(ptr), ram.data()); \
    EXPECT_LT((uint8_t *)(ptr), ram.data() + ram.size())

class RamAllocTest : public RamTestBase
{
};

// --- falloc() basic allocation ---
TEST_F(RamAllocTest, AllocateSinglePage)
{
    void *ptr = falloc(1);
    CHECK_BOUNDARIES(ptr);
}

// --- falloc() should return unique regions ---
TEST_F(RamAllocTest, AllocateMultiplePages)
{
    void *first = falloc(1);
    void *second = falloc(1);
    CHECK_BOUNDARIES(first);
    CHECK_BOUNDARIES(second);
    EXPECT_NE(first, second);
}

// --- falloc() should return nullptr if not enough space ---
TEST_F(RamAllocTest, AllocationFailsOnAllRam)
{
    // Calculate total frames in memory
    const tRam *ram_state = get_ram_state();
    ASSERT_NE(ram_state, nullptr);
    uint16_t total_frames = ram_state->size / ram_state->page_size;

    // Allocate all pages should fail as there are some frames already occupied
    void *ptr = falloc(total_frames);
    EXPECT_EQ(ptr, nullptr);
}

// --- falloc() should return nullptr if not enough space ---
TEST_F(RamAllocTest, AllocationFailsWhenNotEnoughSpace)
{
    // Calculate total frames in memory
    const tRam *ram_state = get_ram_state();
    ASSERT_NE(ram_state, nullptr);
    uint16_t total_frames = ram_state->size / ram_state->page_size;
    uint16_t used_pages = getOccupiedFrames(false, nullptr);

    // Allocate all remaining pages
    void *ptr = falloc(total_frames - used_pages);
    CHECK_BOUNDARIES(ptr);
    uint16_t occupied_frames = getOccupiedFrames(false, nullptr);
    EXPECT_EQ(occupied_frames, total_frames);

    ptr = falloc(1);
    EXPECT_EQ(ptr, nullptr);
}

// --- falloc() should return nullptr if not enough space ---
TEST_F(RamAllocTest, FreeSmallBlockWillNotFitBigger)
{
    // Calculate total frames in memory
    const tRam *ram_state = get_ram_state();
    ASSERT_NE(ram_state, nullptr);
    uint16_t total_frames = ram_state->size / ram_state->page_size;
    uint16_t used_pages = getOccupiedFrames(false, nullptr);

    // Allocate all remaining pages
    void *ptr = falloc(total_frames - used_pages);
    CHECK_BOUNDARIES(ptr);
    uint16_t occupied_frames = getOccupiedFrames(false, nullptr);
    EXPECT_EQ(occupied_frames, total_frames);

    ffree((void *)&ram[10], 1);

    ptr = falloc(2);
    EXPECT_EQ(ptr, nullptr);
}

// --- falloc() should return nullptr if not enough space ---
TEST_F(RamAllocTest, FreeBlockWillFitNew)
{
    // Calculate total frames in memory
    const tRam *ram_state = get_ram_state();
    ASSERT_NE(ram_state, nullptr);
    uint16_t total_frames = ram_state->size / ram_state->page_size;
    uint16_t used_pages = getOccupiedFrames(false, nullptr);

    // Allocate all remaining pages
    void *ptr = falloc(total_frames - used_pages);
    CHECK_BOUNDARIES(ptr);
    uint16_t occupied_frames = getOccupiedFrames(false, nullptr);
    EXPECT_EQ(occupied_frames, total_frames);

    ffree((void *)&ram[10], 2);

    ptr = falloc(2);
    EXPECT_NE(ptr, nullptr);
}

// --- ffree() should release memory that can then be reallocated ---
TEST_F(RamAllocTest, FreeThenReallocate)
{
    void *ptr = falloc(2);
    ASSERT_NE(ptr, nullptr);

    ffree(ptr, 2);

    // Reallocate again
    void *new_ptr = falloc(2);
    CHECK_BOUNDARIES(new_ptr);
    EXPECT_EQ(ptr, new_ptr) << "Expected freed memory to be reusable";
}

// --- ffree() on invalid pointer should not crash ---
TEST_F(RamAllocTest, FreeInvalidPointerDoesNotCrash)
{
    uint8_t fake_region[PAGE_SIZE];
    EXPECT_NO_FATAL_FAILURE(ffree(fake_region, 1));
}

// --- ffree() partially frees memory ---
TEST_F(RamAllocTest, PartialFreeAndReallocate)
{
    void *first = falloc(4);
    ASSERT_NE(first, nullptr);

    // Free middle portion (this may vary depending on implementation)
    ffree(static_cast<uint8_t *>(first) + PAGE_SIZE * 1, 2);

    // Allocate 2 pages (should fit into the freed section)
    void *middle = falloc(2);
    ASSERT_NE(middle, nullptr);
    EXPECT_EQ(middle, static_cast<uint8_t *>(first) + PAGE_SIZE * 1);
}

// --- Allocation without init should fail ---
TEST(RamUninitializedTest, FallocFailsIfUninitialized)
{
    void *ptr = falloc(1);
    EXPECT_EQ(ptr, nullptr);
}

TEST(RamUninitializedTest, FfreeDoesNotCrashIfUninitialized)
{
    uint8_t data[8];
    EXPECT_NO_FATAL_FAILURE(ffree(data, 1));
}

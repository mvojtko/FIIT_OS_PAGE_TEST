#include <cstring>

#include "gtest/gtest.h"
#include "test_ram.h"

extern "C" {
#include "ram.h"
}

#define CHECK_BOUNDARIES(ret, frame_id) \
    ASSERT_EQ((ret), 0);                \
    EXPECT_GE((frame_id), 0);           \
    EXPECT_LT((frame_id), NUM_FRAMES)

class RamAllocTest : public RamTestBase
{
};

// --- falloc() basic allocation ---
TEST_F(RamAllocTest, AllocateSinglePage)
{
    uint16_t frame_id = 0;
    int ret = falloc(&frame_id, 1);
    CHECK_BOUNDARIES(ret, frame_id);
}

// --- falloc() should return unique regions ---
TEST_F(RamAllocTest, AllocateMultiplePages)
{
    uint16_t frame_id1 = 0;
    uint16_t frame_id2 = 0;
    int ret1 = falloc(&frame_id1, 1);
    int ret2 = falloc(&frame_id2, 1);
    CHECK_BOUNDARIES(ret1, frame_id1);
    CHECK_BOUNDARIES(ret2, frame_id2);
    EXPECT_NE(frame_id1, frame_id2);
}

// TODO test wrong parameters

// --- falloc() should return nullptr if not enough space ---
TEST_F(RamAllocTest, AllocationFailsOnAllRam)
{
    // Calculate total frames in memory
    const tRam *ram_state = get_ram_state();
    ASSERT_NE(ram_state, nullptr);
    uint16_t total_frames = ram_state->size / ram_state->page_size;

    // Allocate all pages should fail as there are some frames already occupied
    uint16_t frame_id = 0;
    int ret = falloc(&frame_id, total_frames);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(frame_id, 0);
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
    uint16_t frame_id = 0;
    int ret = falloc(&frame_id, total_frames - used_pages);
    CHECK_BOUNDARIES(ret, frame_id);
    uint16_t occupied_frames = getOccupiedFrames(false, nullptr);
    EXPECT_EQ(occupied_frames, total_frames);

    frame_id = 0;
    ret = falloc(&frame_id, 1);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(frame_id, 0);
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
    uint16_t frame_id = 0;
    int ret = falloc(&frame_id, total_frames - used_pages);
    CHECK_BOUNDARIES(ret, frame_id);
    uint16_t occupied_frames = getOccupiedFrames(false, nullptr);
    EXPECT_EQ(occupied_frames, total_frames);

    ffree(10, 1);

    frame_id = 0;
    ret = falloc(&frame_id, 2);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(frame_id, 0);
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
    uint16_t frame_id = 0;
    int ret = falloc(&frame_id, total_frames - used_pages);
    CHECK_BOUNDARIES(ret, frame_id);
    uint16_t occupied_frames = getOccupiedFrames(false, nullptr);
    EXPECT_EQ(occupied_frames, total_frames);

    ffree(10, 2);

    frame_id = 0;
    ret = falloc(&frame_id, 2);
    CHECK_BOUNDARIES(ret, frame_id);
}

// --- ffree() should release memory that can then be reallocated ---
TEST_F(RamAllocTest, FreeThenReallocate)
{
    uint16_t frame_id1 = 0;
    int ret = falloc(&frame_id1, 2);
    CHECK_BOUNDARIES(ret, frame_id1);

    ffree(frame_id1, 2);

    // Reallocate again
    uint16_t frame_id2 = 0;
    ret = falloc(&frame_id2, 2);
    CHECK_BOUNDARIES(ret, frame_id2);
    EXPECT_EQ(frame_id1, frame_id2) << "Expected freed memory to be reusable";
}

// --- ffree() on invalid pointer should not crash ---
TEST_F(RamAllocTest, FreeInvalidFrameIdDoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE(ffree(NUM_FRAMES, 1));
}

// --- ffree() partially frees memory ---
TEST_F(RamAllocTest, PartialFreeAndReallocate)
{
    uint16_t frame_id1 = 0;
    int ret = falloc(&frame_id1, 4);
    ASSERT_EQ(ret, 0);

    // Free middle portion (this may vary depending on implementation)
    ffree(frame_id1 + 1, 2);

    // Allocate 2 pages (should fit into the freed section)
    uint16_t frame_id2 = 0;
    ret = falloc(&frame_id2, 2);
    ASSERT_EQ(ret, 0);
    EXPECT_EQ(frame_id1 + 1, frame_id2);
}

// --- Allocation without init should fail ---
TEST(RamUninitializedTest, FallocFailsIfUninitialized)
{
    uint16_t frame_id = 0;
    int ret = falloc(&frame_id, 1);
    EXPECT_EQ(ret, -1);
    EXPECT_EQ(frame_id, 0);
}

TEST(RamUninitializedTest, FfreeDoesNotCrashIfUninitialized)
{
    EXPECT_NO_FATAL_FAILURE(ffree(0, 1));
}

#include <cstring>  // for memset

#include "gtest/gtest.h"
#include "test_ram.h"

extern "C" {
#include "pager.h"
#include "task.h"
#include "types.h"
}

// GoogleTest fixture for pager tests
class PagerTest : public RamTestBase
{
  protected:
    void SetUp() override
    {
        init_taskMgr();
        tPageTableEntry page_table[PAGE_TABLE_SIZE];
        // Prepare a simple page table for testing
        memset(&page_table, 0, sizeof(page_table));
        memset(address_space, 0, sizeof(address_space));

        // Page 0: fully accessible and already in memory
        page_table[0].r = 1;
        page_table[0].w = 1;
        page_table[0].x = 1;
        page_table[0].p_bit = 1;
        page_table[0].frame_id = 1;

        // Page 1: read-only, not present yet
        page_table[1].r = 1;
        page_table[1].w = 0;
        page_table[1].x = 0;
        page_table[1].p_bit = 0;
        page_table[1].frame_id = 2;

        // Page 2: read-write, not present yet
        page_table[1].r = 1;
        page_table[1].w = 1;
        page_table[1].x = 0;
        page_table[1].p_bit = 0;
        page_table[1].frame_id = 0;

        // Initialize task (assume success)
        pid = create_task(page_table, 4, address_space);
        ASSERT_GE(pid, 0) << "Failed to create task before running pager tests.";
    }

    void TearDown()
    {
        destroy_taskMgr();
    }

    int pid;
    uint8_t address_space[PAGE_SIZE*PAGE_TABLE_SIZE] = {0};  // Example page data
};

// --- page_fault() tests ---

TEST_F(PagerTest, LoadPageSuccess)
{
    int result = page_fault(pid, 55);
    EXPECT_EQ(result, 0) << "Expected success when loading a valid page.";
}

TEST_F(PagerTest, LoadPageOutOfFrames)
{
    int result = page_fault(pid, 99);
    EXPECT_EQ(result, -1) << "Expected out of page frames.";
}

TEST_F(PagerTest, LoadPageSegmentationFault)
{
    int result = 0;
    EXPECT_NO_FATAL_FAILURE(result = page_fault(pid, 1));
    EXPECT_EQ(result, -2) << "Expected segmentation fault when data is null.";
}

TEST_F(PagerTest, LoadPageTaskNotFound)
{
    int result = page_fault(pid, 14);
    EXPECT_EQ(result, -3) << "Expected task not found error.";
}

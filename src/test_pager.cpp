#include <cstring>  // for memset

#include "gtest/gtest.h"
#include "test_ram.h"

extern "C" {
#include "pager.h"
#include "task.h"
#include "types.h"
}

class PagerTest : public RamTestBase
{
  protected:
    void SetUp() override
    {
        init_taskMgr();
        tPageTableEntry page_table[PAGE_TABLE_SIZE];
        // Prepare empty page_table
        memset(&page_table, 0, sizeof(page_table));

        // Init address space of task so each page has different content
        for (uint8_t id = 0; id < PAGE_TABLE_SIZE + 1; id++)
        {
            memset(address_space + PAGE_SIZE * id, 0xcd + id, sizeof(PAGE_SIZE));
        }

        // Initialize task (assume success)
        pid = create_task(page_table, 2, address_space);
        ASSERT_GE(pid, 0) << "Failed to create task before running pager tests.";
        task = get_task_struct(pid);
        ASSERT_NE(task, nullptr);
    }

    void SetWritablePageEntry(uint16_t page_id)
    {
        task->page_table[page_id].w = 0x1;
    }

    void SetPresentPageEntry(uint16_t page_id)
    {
        task->page_table[page_id].p_bit = 0x1;
    }

    void CheckPagePresentInRam(uint8_t page_id, const char *text = nullptr)
    {
        const uint8_t p_bit = task->page_table[page_id].p_bit;
        ASSERT_EQ(p_bit, 0x1) << "Page Expected in ram";

        const uint16_t frame_id = task->page_table[page_id].frame_id;
        ASSERT_LT(frame_id, NUM_FRAMES) << "Stored frame_id is outside of reserved memory for ram";

        const uint8_t *frame = (ram + task->page_table[page_id].frame_id * PAGE_SIZE);
        const uint8_t *page = (address_space + page_id * PAGE_SIZE);
        text = (text) ? text : "Expected that content of page is written to ram";
        const bool frame_and_page_equal = memcmp(frame, page, PAGE_SIZE) == 0;
        EXPECT_EQ(frame_and_page_equal, true) << text;
    }

    void TearDown() override
    {
        destroy_taskMgr();
    }

    int pid;
    tTaskStruct *task;
    uint8_t address_space[PAGE_SIZE * (PAGE_TABLE_SIZE + 1)];
};

class NRUTest : public PagerTest
{
  protected:
    void SetUp() override
    {
        PagerTest::SetUp();
        SetWritablePageEntry(1);
        SetWritablePageEntry(2);
        SetWritablePageEntry(7);
        int result = page_fault(pid, PAGE_SIZE * 1);
        ASSERT_EQ(result, 0);
        result = page_fault(pid, PAGE_SIZE * 2);
        ASSERT_EQ(result, 0);
    }
};

// --- page_fault() tests ---

TEST_F(PagerTest, PageFaultPageNotAccessible)
{
    int result = page_fault(pid, PAGE_SIZE);
    EXPECT_EQ(result, -4) << "Expected segmentation fault";
}

TEST_F(PagerTest, PageFaultAlreadyPresent)
{
    SetWritablePageEntry(0);
    SetPresentPageEntry(0);
    int result = page_fault(pid, 0);
    EXPECT_EQ(result, -2) << "Expected fail when loading page that is already present.";
}

TEST_F(PagerTest, PageFaultOutOfFrames)
{
    SetWritablePageEntry(0);
    // Calculate total frames in memory
    const tRam *ram_state = get_ram_state();
    ASSERT_NE(ram_state, nullptr);
    const uint16_t used_frames = getOccupiedFrames(nullptr);

    // Allocate all remaining pages
    uint16_t frame_id = 0;
    int ret = falloc(&frame_id, NUM_FRAMES - used_frames);
    ASSERT_EQ(ret, 0);

    int result = page_fault(pid, 0);
    EXPECT_EQ(result, -3) << "Expected out of page frames.";
}

TEST_F(PagerTest, PageFaultSegmentationFault)
{
    int result = 0;
    EXPECT_NO_FATAL_FAILURE(result = page_fault(pid, PAGE_SIZE * 8));
    EXPECT_EQ(result, -4) << "Expected segmentation fault when accessing page_id bigger than page table size";
}

TEST_F(PagerTest, PageFaultRamNotInitialized)
{
    destroy_ram();
    int result = page_fault(pid, 14);
    EXPECT_EQ(result, -1) << "Expected task not found error.";
}

TEST_F(PagerTest, PageFaultTaskMgrNotInitialized)
{
    destroy_taskMgr();
    int result = page_fault(pid, 14);
    EXPECT_EQ(result, -1) << "Expected task not found error.";
}

TEST_F(PagerTest, PageFaultTaskDestroyed)
{
    destroy_task(pid);
    int result = page_fault(pid, 14);
    EXPECT_EQ(result, -1) << "Expected task not found error.";
}

TEST_F(PagerTest, PageFaultSuccess)
{
    SetWritablePageEntry(1);
    int result = page_fault(pid, PAGE_SIZE);
    EXPECT_EQ(result, 0) << "Expected success when loading a valid page.";
    uint8_t p_bit = task->page_table[1].p_bit;
    EXPECT_EQ(p_bit, 0x1) << "Expected page entry should have p_bit set";
}

TEST_F(PagerTest, PageFaultSuccessMorePages)
{
    SetWritablePageEntry(1);
    SetWritablePageEntry(3);
    int result = page_fault(pid, PAGE_SIZE);
    ASSERT_EQ(result, 0);
    result = page_fault(pid, PAGE_SIZE * 3);
    EXPECT_EQ(result, 0) << "Expected success when loading a valid page.";
    uint8_t p_bit = task->page_table[3].p_bit;
    EXPECT_EQ(p_bit, 0x1) << "Expected page entry should have p_bit set";
}

TEST_F(PagerTest, PageFaultWritesPageToRam)
{
    SetWritablePageEntry(1);
    int result = page_fault(pid, PAGE_SIZE);
    ASSERT_EQ(result, 0);

    CheckPagePresentInRam(1);
}

TEST_F(PagerTest, PageFaultWritesMorePagesToRam)
{
    SetWritablePageEntry(1);
    SetWritablePageEntry(7);
    int result = page_fault(pid, PAGE_SIZE);
    ASSERT_EQ(result, 0);
    result = page_fault(pid, PAGE_SIZE * 7);
    ASSERT_EQ(result, 0);

    CheckPagePresentInRam(1);
    CheckPagePresentInRam(7);
}

TEST_F(NRUTest, NoRecentNorModifiedPages)
{
    uint16_t frame_id1 = task->page_table[1].frame_id;
    uint16_t frame_id2 = task->page_table[2].frame_id;
    int result = page_fault(pid, PAGE_SIZE * 7);
    EXPECT_EQ(result, 0);
    uint8_t p_bit1 = task->page_table[1].p_bit;
    uint8_t p_bit2 = task->page_table[2].p_bit;
    uint8_t p_bit7 = task->page_table[7].p_bit;
    uint16_t frame_id7 = task->page_table[7].frame_id;
    EXPECT_NE(p_bit1, p_bit2) << "Expected one of present pages to be evicted";
    EXPECT_EQ(p_bit7, 0x1) << "Expected new page present in ram";
    uint16_t evicted_frame_id = (p_bit1 == 0) ? frame_id1 : frame_id2;
    EXPECT_EQ(frame_id7, evicted_frame_id) << "Expected frame_id of evicted page used by new page";
    CheckPagePresentInRam(7);
}

TEST_F(NRUTest, OneRecentPage)
{
    uint16_t frame_id2 = task->page_table[2].frame_id;
    task->page_table[1].r_bit = 0x1;
    int result = page_fault(pid, PAGE_SIZE * 7);
    EXPECT_EQ(result, 0);
    uint8_t p_bit1 = task->page_table[1].p_bit;
    uint8_t r_bit1 = task->page_table[1].r_bit;
    uint8_t p_bit2 = task->page_table[2].p_bit;
    uint8_t p_bit7 = task->page_table[7].p_bit;
    uint16_t frame_id7 = task->page_table[7].frame_id;
    EXPECT_EQ(p_bit2, 0x0) << "Expected not accessed page to be evicted";
    EXPECT_EQ(p_bit1, 0x1) << "Expected recent page to stay";
    EXPECT_EQ(r_bit1, 0x0) << "Expected recent page r_bit cleared";
    EXPECT_EQ(p_bit7, 0x1) << "Expected new page present in ram";
    EXPECT_EQ(frame_id7, frame_id2) << "Expected frame_id of evicted page used by new page";
    CheckPagePresentInRam(7);
}

TEST_F(NRUTest, OneModifiedPage)
{
    uint16_t frame_id1 = task->page_table[1].frame_id;
    uint16_t frame_id2 = task->page_table[2].frame_id;
    task->page_table[2].m_bit = 0x1;
    ram[frame_id2 * PAGE_SIZE + frame_id2] = frame_id2;

    int result = page_fault(pid, PAGE_SIZE * 7);
    EXPECT_EQ(result, 0);
    uint8_t p_bit1 = task->page_table[1].p_bit;
    uint8_t m_bit2 = task->page_table[2].m_bit;
    uint8_t p_bit2 = task->page_table[2].p_bit;
    uint8_t p_bit7 = task->page_table[7].p_bit;
    uint16_t frame_id7 = task->page_table[7].frame_id;
    EXPECT_EQ(p_bit1, 0x0) << "Expected not accessed page to be evicted";
    EXPECT_EQ(p_bit2, 0x1) << "Expected modified page to stay";
    EXPECT_EQ(m_bit2, 0x0) << "Expected modified page m_bit cleared";
    EXPECT_EQ(p_bit7, 0x1) << "Expected new page present in ram";
    EXPECT_EQ(frame_id7, frame_id1) << "Expected frame_id of evicted page used by new page";
    CheckPagePresentInRam(7);
    CheckPagePresentInRam(2, "Expected modified page written back to tasks address_space");
}

TEST_F(NRUTest, AllRecentPages)
{
    uint16_t frame_id1 = task->page_table[1].frame_id;
    uint16_t frame_id2 = task->page_table[2].frame_id;
    task->page_table[1].r_bit = 0x1;
    task->page_table[2].r_bit = 0x1;

    int result = page_fault(pid, PAGE_SIZE * 7);
    EXPECT_EQ(result, 0);
    uint8_t r_bit1 = task->page_table[1].r_bit;
    uint8_t p_bit1 = task->page_table[1].p_bit;
    uint8_t r_bit2 = task->page_table[2].r_bit;
    uint8_t p_bit2 = task->page_table[2].p_bit;
    uint8_t p_bit7 = task->page_table[7].p_bit;
    uint16_t frame_id7 = task->page_table[7].frame_id;
    EXPECT_NE(p_bit1, p_bit2) << "Expected one of pages to be evicted";
    EXPECT_EQ(r_bit1, 0x0) << "Expected recent page r_bit cleared";
    EXPECT_EQ(r_bit2, 0x0) << "Expected recent page r_bit cleared";
    EXPECT_EQ(p_bit7, 0x1) << "Expected new page present in ram";
    uint16_t evicted_frame_id = (p_bit1) ? frame_id2 : frame_id1;
    EXPECT_EQ(frame_id7, evicted_frame_id) << "Expected frame_id of evicted page used by new page";
    CheckPagePresentInRam(7);
}

TEST_F(NRUTest, AllModifiedPages)
{
    uint16_t frame_id1 = task->page_table[1].frame_id;
    uint16_t frame_id2 = task->page_table[2].frame_id;
    task->page_table[1].m_bit = 0x1;
    task->page_table[2].m_bit = 0x1;
    ram[frame_id1 * PAGE_SIZE + frame_id1] = frame_id1;
    ram[frame_id2 * PAGE_SIZE + frame_id2] = frame_id2;

    int result = page_fault(pid, PAGE_SIZE * 7);
    EXPECT_EQ(result, 0);
    uint8_t m_bit1 = task->page_table[1].m_bit;
    uint8_t p_bit1 = task->page_table[1].p_bit;
    uint8_t m_bit2 = task->page_table[2].m_bit;
    uint8_t p_bit2 = task->page_table[2].p_bit;
    uint8_t p_bit7 = task->page_table[7].p_bit;
    uint16_t frame_id7 = task->page_table[7].frame_id;
    EXPECT_NE(p_bit1, p_bit2) << "Expected one of pages to be evicted";
    EXPECT_EQ(m_bit1, 0x0) << "Expected modified page m_bit cleared";
    EXPECT_EQ(m_bit2, 0x0) << "Expected modified page m_bit cleared";
    EXPECT_EQ(p_bit7, 0x1) << "Expected new page present in ram";
    const uint16_t evicted_frame_id = (p_bit1) ? frame_id2 : frame_id1;
    const uint16_t evicted_page_id = (p_bit1) ? 2 : 1;
    EXPECT_EQ(frame_id7, evicted_frame_id) << "Expected frame_id of evicted page used by new page";
    CheckPagePresentInRam(7);
    CheckPagePresentInRam((p_bit1) ? 1 : 2, "Expected modified page written back to tasks address_space");
    bool address_space_modified = address_space[PAGE_SIZE * evicted_page_id + evicted_frame_id] == evicted_frame_id;
    EXPECT_EQ(address_space_modified, true) << "Expected that modified and evicted page written to address_space";
}

TEST_F(NRUTest, OneRecentAndOneModifiedPage)
{
    uint16_t frame_id1 = task->page_table[1].frame_id;
    uint16_t frame_id2 = task->page_table[2].frame_id;
    task->page_table[1].m_bit = 0x1;
    task->page_table[2].r_bit = 0x1;
    ram[frame_id1 * PAGE_SIZE + frame_id1] = frame_id1;

    int result = page_fault(pid, PAGE_SIZE * 7);
    EXPECT_EQ(result, 0);
    uint8_t r_bit1 = task->page_table[1].m_bit;
    uint8_t p_bit1 = task->page_table[1].p_bit;
    uint8_t r_bit2 = task->page_table[2].m_bit;
    uint8_t p_bit2 = task->page_table[2].p_bit;
    uint8_t p_bit7 = task->page_table[7].p_bit;
    uint16_t frame_id7 = task->page_table[7].frame_id;
    EXPECT_NE(p_bit1, p_bit2) << "Expected one of pages to be evicted";
    EXPECT_EQ(r_bit1, 0x0) << "Expected recent page r_bit cleared";
    EXPECT_EQ(r_bit2, 0x0) << "Expected recent page r_bit cleared";
    EXPECT_EQ(p_bit7, 0x1) << "Expected new page present in ram";
    uint16_t evicted_frame_id = (p_bit1) ? frame_id2 : frame_id1;
    EXPECT_EQ(frame_id7, evicted_frame_id) << "Expected frame_id of evicted page used by new page";
    CheckPagePresentInRam(7);
    bool address_space_modified = address_space[PAGE_SIZE * 1 + frame_id1] == frame_id1;
    EXPECT_EQ(address_space_modified, true) << "Expected that modified and evicted page written to address_space";
}

TEST_F(NRUTest, OneRecentModifiedAndOneRecentPage)
{
    uint16_t frame_id1 = task->page_table[1].frame_id;
    uint16_t frame_id2 = task->page_table[2].frame_id;
    task->page_table[1].m_bit = 0x1;
    task->page_table[1].r_bit = 0x1;
    task->page_table[2].r_bit = 0x1;
    ram[frame_id1 * PAGE_SIZE + frame_id1] = frame_id1;

    int result = page_fault(pid, PAGE_SIZE * 7);
    EXPECT_EQ(result, 0);
    uint8_t r_bit1 = task->page_table[1].m_bit;
    uint8_t p_bit1 = task->page_table[1].p_bit;
    uint8_t r_bit2 = task->page_table[2].m_bit;
    uint8_t p_bit2 = task->page_table[2].p_bit;
    uint8_t p_bit7 = task->page_table[7].p_bit;
    uint16_t frame_id7 = task->page_table[7].frame_id;
    EXPECT_NE(p_bit1, p_bit2) << "Expected one of pages to be evicted";
    EXPECT_EQ(p_bit2, 0x0);
    EXPECT_EQ(r_bit1, 0x0) << "Expected recent page r_bit cleared";
    EXPECT_EQ(r_bit2, 0x0) << "Expected recent page r_bit cleared";
    EXPECT_EQ(p_bit7, 0x1) << "Expected new page present in ram";
    uint16_t evicted_frame_id = frame_id2;
    EXPECT_EQ(frame_id7, evicted_frame_id) << "Expected frame_id of evicted page used by new page";
    CheckPagePresentInRam(7);
    bool address_space_modified = address_space[PAGE_SIZE * 1 + frame_id1] == frame_id1;
    EXPECT_EQ(address_space_modified, true) << "Expected that modified and evicted page written to address_space";
}

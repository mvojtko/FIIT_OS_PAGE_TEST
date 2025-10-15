#include "gtest/gtest.h"
#include "test_ram.h"

extern "C" {
#include "task.h"
}

bool operator==(const tPageTableEntry &lhs, const tPageTableEntry &rhs)
{
    return memcmp(&lhs, &rhs, sizeof(tPageTableEntry)) == 0;
}


void PrintTo(const tPageTableEntry& entry, std::ostream* os) {
    *os << "rwx=" << std::to_string(entry.r) << std::to_string(entry.w) << std::to_string(entry.x) <<
           ", m_bit=" << std::to_string(entry.m_bit) <<
           ", p_bit=" << std::to_string(entry.p_bit) <<
           ", r_bit=" << std::to_string(entry.r_bit) <<
           ", frame_id=" << std::to_string(entry.frame_id);
}

// Helper: ensure clean state before and after each test
class TaskManagerTest : public RamTestBase
{
  protected:
    void SetUp() override
    {
        page_table.resize(8);
        memset(page_table.data(), 1, page_table.size() * sizeof(tPageTableEntry));
        memset(address_space, 2, sizeof(address_space));
        init_taskMgr();
    }

    void TearDown() override
    {
        destroy_taskMgr();
    }

    std::vector<tPageTableEntry> page_table;
    uint8_t address_space[PAGE_SIZE * PAGE_TABLE_SIZE];
};

TEST(TaskManagerTest_NoRam, NoManager)
{
    EXPECT_EQ(nullptr, get_task_mgr());
}

TEST(TaskManagerTest_NoRam, InitManagerWithoutRamInit)
{
    EXPECT_EQ(init_taskMgr(), -1);
}

TEST(TaskManagerTest_NoRam, InitManagerNotEnoughResources)
{
    uint8_t buffer[nextPow2(sizeof(tRam))];
    init_ram(&buffer, nextPow2(sizeof(tRam)), nextPow2(sizeof(tRam)));
    EXPECT_EQ(init_taskMgr(), -1);
}

TEST_F(TaskManagerTest, InitAndDestroyManager)
{
    // Ensure init_taskMgr initializes the manager correctly
    destroy_taskMgr();  // destroy first to test clean init
    EXPECT_EQ(init_taskMgr(), 0) << "Task manager should initialize successfully";

    const tTaskMgr *mgr = get_task_mgr();
    ASSERT_NE(mgr, nullptr) << "Manager should not be null after init";

    destroy_taskMgr();
    EXPECT_EQ(get_task_mgr(), nullptr) << "Manager should be null after destroy";
}

TEST_F(TaskManagerTest, CreateTaskSuccessfully)
{
    uint8_t max_frames = 4;
    int pid = create_task(page_table.data(), max_frames, address_space);
    EXPECT_GE(pid, 0) << "create_task should return valid pid >= 0";

    const tTaskMgr *mgr = get_task_mgr();
    ASSERT_NE(mgr, nullptr);

    bool found = false;
    for (const auto &task : mgr->tasks)
    {
        if (task.pid == pid)
        {
            found = true;
            for (size_t id = 0; id < page_table.size(); id++)
            {
                EXPECT_EQ(task.page_table[id], page_table[id]);
            }
            EXPECT_EQ(task.max_frames, max_frames);
            break;
        }
    }
    EXPECT_TRUE(found) << "Created task should appear in task manager";
}

TEST_F(TaskManagerTest, CreateTaskInvalidParams)
{
    // Null page_table should yield -2
    EXPECT_EQ(create_task(nullptr, 5, address_space), -2);
}

TEST_F(TaskManagerTest, CreateTaskWithoutInitFails)
{
    destroy_taskMgr();  // simulate uninitialized state
    EXPECT_EQ(create_task(page_table.data(), 4, address_space), -3);
}

TEST_F(TaskManagerTest, CreateTaskResourceLimit)
{
    // Create max number of tasks
    for (int i = 0; i < 8; ++i)
    {
        EXPECT_GE(create_task(page_table.data(), 1, address_space), 0);
    }

    // Creating one more should fail
    EXPECT_EQ(create_task(page_table.data(), 1, address_space), -1);
}

TEST_F(TaskManagerTest, DestroyTaskWithoutInitFails)
{
    destroy_taskMgr();  // simulate uninitialized state
    EXPECT_EQ(destroy_task(1), -1);
}

TEST_F(TaskManagerTest, DestroyExistingTask)
{
    int pid = create_task(page_table.data(), 2, address_space);
    ASSERT_GE(pid, 0);

    EXPECT_EQ(destroy_task(pid), 0);

    // Task should be gone
    const tTaskMgr *mgr = get_task_mgr();
    bool still_exists = false;
    for (const auto &task : mgr->tasks)
    {
        if (task.pid == pid)
            still_exists = true;
    }
    EXPECT_FALSE(still_exists);
}

TEST_F(TaskManagerTest, DestroyNonexistentTask)
{
    EXPECT_EQ(destroy_task(5), -1);
    EXPECT_EQ(destroy_task(9999), -1);
}

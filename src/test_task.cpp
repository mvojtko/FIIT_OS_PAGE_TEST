#include "gtest/gtest.h"

extern "C" {
#include "task.h"
}

// Helper: ensure clean state before and after each test
class TaskManagerTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        task_mgr.tasks = new tTaskStruct[16];
        task_mgr.size = 16;
        init_taskMgr(&task_mgr);
    }

    void TearDown() override
    {
        destroy_taskMgr();
    }

    tTaskMgr task_mgr;
};

TEST(TaskManagerTest_Init, init_taskMgr__invalid_params)
{
    tTaskStruct tasks[8];
    tTaskMgr task_mgr { .tasks = nullptr, .size = 0 };
    // Ensure init_taskMgr initializes the manager correctly
    EXPECT_EQ(init_taskMgr(nullptr), -1) << "init_taskMgr should return -1 for nullptr";
    EXPECT_EQ(init_taskMgr(&task_mgr), -1) << "init_taskMgr there should be some space for tasks";

    task_mgr.size = 8;
    EXPECT_EQ(init_taskMgr(&task_mgr), -1) << "init_taskMgr there is size set but the tasks array is nullptr";

    task_mgr.size = 0;
    task_mgr.tasks = tasks;
    EXPECT_EQ(init_taskMgr(&task_mgr), -1) << "init_taskMgr there is some space for tasks but the size is 0";
}

TEST_F(TaskManagerTest, init_taskMgr__clears_tasks_structs)
{
    EXPECT_EQ(task_mgr.tasks[6].pid, -1) << "expected pid -1";
    EXPECT_EQ(task_mgr.tasks[6].ppid, 0) << "expected ppid 0";
    EXPECT_EQ(task_mgr.tasks[6].priority, 0) << "expected priority 0";
    EXPECT_EQ(task_mgr.tasks[6].signals, 0) << "expected signals 0";
    EXPECT_EQ(task_mgr.tasks[6].state, 0) << "expected state 0";
    EXPECT_EQ(task_mgr.tasks[6].exit_code, 0) << "expected exit code 0";
    EXPECT_EQ(task_mgr.tasks[6].runtime, 0) << "expected runtime 0";
    EXPECT_EQ(task_mgr.tasks[6].uid, 0) << "expected uid 0";
}

TEST_F(TaskManagerTest, create_task__invalid_params)
{
    EXPECT_EQ(create_task(0, 0, 0), -2);
}

TEST_F(TaskManagerTest, create_task__without_init_fails)
{
    destroy_taskMgr();  // simulate uninitialized state
    EXPECT_EQ(create_task(0, 0, 0), -3) << "expected -3";
}

TEST_F(TaskManagerTest, create_task__resource_limit)
{

}

TEST_F(TaskManagerTest, destroy_task__without_init_fails)
{
    destroy_taskMgr();  // simulate uninitialized state
    EXPECT_EQ(destroy_task(1), -1);
}

TEST_F(TaskManagerTest, destroy_task__existing_task)
{
}

TEST_F(TaskManagerTest, destroy_task__nonexistent_task)
{
    EXPECT_EQ(destroy_task(5), -1);
    EXPECT_EQ(destroy_task(9999), -1);
}

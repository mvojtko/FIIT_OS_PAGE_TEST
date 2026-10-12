#include "gtest/gtest.h"
#include "debug.h"
#include "test_common.h"

// Helper: ensure clean state before and after each test
class TaskManagerTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        memset(tasks, 0xcd, sizeof(tasks));
        task_mgr.tasks = tasks + 1;
        task_mgr.size = 16;
        init_taskMgr(&task_mgr);
        CheckOverflow();
    }

    void TearDown() override
    {
        CheckOverflow();
        destroy_taskMgr();
        CheckOverflow();
    }

    void CheckOverflow() const
    {
        tTaskStruct expected;
        memset(&expected, 0xcd, sizeof(tTaskStruct));
        auto selected = tasks[0];
        EXPECT_EQ(selected, expected) << "tasks init overflow";
        selected = tasks[NUMS_ELEMENTS(tasks) - 1];
        EXPECT_EQ(selected, expected) << "tasks init overflow";
    }

    const tTaskStruct *GetTask(tPid pid) const
    {
        for (uint i = 0; i < NUMS_ELEMENTS(tasks); i++)
        {
            if (tasks[i].pid == pid)
            {
                return &tasks[i];
            }
        }
        return nullptr;
    }

    tTaskStruct tasks[18];
    tTaskMgr task_mgr;
};

TEST(TaskManagerTest_Init, init_taskMgr__invalid_params)
{
    tTaskStruct tasks[8];
    tTaskMgr task_mgr { .tasks = nullptr, .size = 0 };
    // Ensure init_taskMgr initializes the manager correctly
    EXPECT_EQ(init_taskMgr(nullptr), -1) << "init_taskMgr should return -1 for nullptr";
    EXPECT_EQ(init_taskMgr(&task_mgr), -1) << "init_taskMgr tTaskMgr.tasks can not be nullptr";

    task_mgr.size = 8;
    EXPECT_EQ(init_taskMgr(&task_mgr), -1) << "init_taskMgr tTaskMgr.tasks can not be nullptr";

    task_mgr.size = 0;
    task_mgr.tasks = tasks;
    EXPECT_EQ(init_taskMgr(&task_mgr), -1) << "init_taskMgr tTaskMgr.size can not be zero";
}

TEST_F(TaskManagerTest, init_taskMgr__clears_tasks_structs)
{
    tTaskStruct expected{
        .pid = 0,
        .ppid = -1,
        .priority = 0,
        .signals = 0,
        .state = 0,
        .uid = 0,
        .runtime = 0,
        .start_time = 0,
        .exit_code = 0
    };
    init_taskMgr(&task_mgr);

    auto selected = task_mgr.tasks[0];
    EXPECT_EQ(selected, expected) << "there is expected task 0 at index 0 created";
    expected.pid = -1;
    selected = task_mgr.tasks[6];
    EXPECT_EQ(selected, expected) << "tasks are not initialized";
    selected = task_mgr.tasks[15];
    EXPECT_EQ(selected, expected) << "tasks are not initialized";
}

TEST_F(TaskManagerTest, get_taskMgr__without_init_fails)
{
    destroy_taskMgr();
    EXPECT_EQ(get_taskMgr(), nullptr) << "system not initialized";
}

TEST_F(TaskManagerTest, get_taskMgr__with_init_returns_pointer)
{
    EXPECT_EQ(get_taskMgr(), &task_mgr) << "system initialized";
}

TEST_F(TaskManagerTest, create_task__invalid_params)
{
    EXPECT_EQ(create_task(0, -1, 0), -2) << "invalid ppid";
    EXPECT_EQ(create_task(16, 0, 0), -2) << "invalid priority";
}

TEST_F(TaskManagerTest, create_task__without_init_fails)
{
    destroy_taskMgr();  // simulate uninitialized state
    EXPECT_EQ(create_task(0, 0, 0), -3) << "system not initialized";
}

TEST_F(TaskManagerTest, create_task__parent_not_found)
{
    EXPECT_EQ(create_task(0, 2, 0), -4) << "parent process not found";
}

TEST_F(TaskManagerTest, create_task__success)
{
    auto pid = create_task(7, 0, 42);
    EXPECT_GT(pid, 0) << "create_task should return a positive PID";

    auto *task = GetTask(pid);
    ASSERT_NE(task, nullptr) << "created task must be stored in the manager";
    EXPECT_EQ(task->pid, pid);
    EXPECT_EQ(task->ppid, 0);
    EXPECT_EQ(task->priority, 7);
    EXPECT_EQ(task->uid, 42);
    EXPECT_EQ(task->signals, 0);
    EXPECT_EQ(task->state, TASK_STATE_READY);
    EXPECT_EQ(task->runtime, 0);
    EXPECT_EQ(task->start_time, 0);
    EXPECT_EQ(task->exit_code, 0);
}

TEST_F(TaskManagerTest, create_task__priority_15_is_valid)
{
    auto pid = create_task(15, 0, 1);
    EXPECT_GT(pid, 0);
    auto *task = GetTask(pid);
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->priority, 15);
}

TEST_F(TaskManagerTest, create_task__with_non_init_parent)
{
    auto parent = create_task(1, 0, 3);
    ASSERT_GT(parent, 0);
    auto child = create_task(2, parent, 4);
    EXPECT_GT(child, 0);
    EXPECT_NE(child, parent);

    auto *task = GetTask(child);
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->ppid, parent);
    EXPECT_EQ(task->uid, 4);
    EXPECT_EQ(task->priority, 2);
}

TEST_F(TaskManagerTest, create_task__unique_pids)
{
    tPid pids[4];
    for (int i = 0; i < 4; i++)
    {
        pids[i] = create_task(0, 0, 0);
        EXPECT_GT(pids[i], 0);
        for (int j = 0; j < i; j++)
        {
            EXPECT_NE(pids[i], pids[j]) << "PIDs must be unique";
        }
    }
}

TEST_F(TaskManagerTest, create_task__resource_limit)
{
    for (uint8_t i = 0; i < task_mgr.size - 1; i++)
    {
        EXPECT_GT(create_task(0, 0, 0), 0);
    }
    EXPECT_EQ(create_task(0, 0, 0), -1) << "resource limit reached";
}

TEST_F(TaskManagerTest, create_task__zombie_parent_fails)
{
    auto parent = create_task(0, 0, 0);
    ASSERT_GT(parent, 0);
    EXPECT_EQ(terminate_task(parent, 1), 0);
    EXPECT_EQ(create_task(0, parent, 0), -4) << "zombie must not become a parent";
}

TEST_F(TaskManagerTest, create_task__destroyed_parent_fails)
{
    auto parent = create_task(0, 0, 0);
    ASSERT_GT(parent, 0);
    EXPECT_EQ(destroy_task(parent), 0);
    EXPECT_EQ(create_task(0, parent, 0), -4) << "destroyed task is not a valid parent";
}

TEST_F(TaskManagerTest, create_task__reuses_slot_after_destroy)
{
    tPid first = create_task(0, 0, 0);
    ASSERT_GT(first, 0);
    for (uint8_t i = 1; i < task_mgr.size - 1; i++)
    {
        EXPECT_GT(create_task(0, 0, 0), 0);
    }
    EXPECT_EQ(create_task(0, 0, 0), -1);

    EXPECT_EQ(destroy_task(first), 0);
    auto reused = create_task(0, 0, 0);
    EXPECT_GT(reused, 0) << "freed slot should be reusable";
    EXPECT_EQ(reused, first) << "freed PID should be reused";
}

TEST_F(TaskManagerTest, create_task__reuses_pid_after_destroy)
{
    tPid pid;
    for (int i = 0; i < 5; i++)
        pid = create_task(0, 0, 0);

    for (int i = 0; i < 5; i++)
        create_task(0, 0, 0);

    destroy_task(pid);
    tPid new_pid = create_task(0, 0, 0);
    EXPECT_EQ(pid, new_pid) << "PID must be reused so the counter cannot grow without bound";
}

TEST_F(TaskManagerTest, create_task__zombie_holds_pid_until_reaped)
{
    auto pid = create_task(0, 0, 0);
    ASSERT_EQ(pid, 1);
    ASSERT_EQ(terminate_task(pid, 0), 0);

    auto other = create_task(0, 0, 0);
    EXPECT_EQ(other, 2) << "zombie still owns its PID";

    ASSERT_EQ(destroy_task(pid), 0);
    auto reused = create_task(0, 0, 0);
    EXPECT_EQ(reused, 1) << "PID is recycled only after the task entry is destroyed";
}

TEST_F(TaskManagerTest, terminate_task__pid_0_fails)
{
    EXPECT_EQ(terminate_task(0, 0), -1) << "task 0 can not be terminated";
}

TEST_F(TaskManagerTest, terminate_task__task_not_found)
{
    EXPECT_EQ(terminate_task(1, 0), -1) << "task 1 not found";
}

TEST_F(TaskManagerTest, terminate_task__existing_task)
{
    auto pid = create_task(3, 0, 9);
    ASSERT_GT(pid, 0);
    EXPECT_EQ(terminate_task(pid, 42), 0) << "task can be terminated";

    auto *task = GetTask(pid);
    ASSERT_NE(task, nullptr) << "zombie must keep its PID";
    EXPECT_EQ(task->pid, pid);
    EXPECT_EQ(task->ppid, 0);
    EXPECT_EQ(task->priority, 3);
    EXPECT_EQ(task->uid, 9);
    EXPECT_EQ(task->state, TASK_STATE_TERMINATED);
    EXPECT_EQ(task->exit_code, 42);

    auto *parent = GetTask(0);
    ASSERT_NE(parent, nullptr);
    EXPECT_EQ(parent->signals & TASK_SIG_CHILD, TASK_SIG_CHILD)
        << "parent should receive TASK_SIG_CHILD";
}

TEST_F(TaskManagerTest, terminate_task__without_init_fails)
{
    destroy_taskMgr();
    EXPECT_EQ(terminate_task(1, 0), -3) << "system not initialized";
}

TEST_F(TaskManagerTest, terminate_task__already_zombie_fails)
{
    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    EXPECT_EQ(terminate_task(pid, 1), 0);
    EXPECT_EQ(terminate_task(pid, 2), -1) << "zombie cannot be terminated again";
}

TEST_F(TaskManagerTest, terminate_task__blocked_task)
{
    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    EXPECT_EQ(block_task(pid), 0);
    EXPECT_EQ(terminate_task(pid, 7), 0);
    auto *task = GetTask(pid);
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->state, TASK_STATE_TERMINATED);
    EXPECT_EQ(task->exit_code, 7);
}

TEST_F(TaskManagerTest, terminate_task__signals_direct_parent_only)
{
    auto parent = create_task(0, 0, 0);
    auto child = create_task(0, parent, 0);
    ASSERT_GT(parent, 0);
    ASSERT_GT(child, 0);

    EXPECT_EQ(terminate_task(child, 0), 0);

    auto *init = GetTask(0);
    auto *parent_task = GetTask(parent);
    ASSERT_NE(init, nullptr);
    ASSERT_NE(parent_task, nullptr);
    EXPECT_EQ(parent_task->signals & TASK_SIG_CHILD, TASK_SIG_CHILD);
    EXPECT_EQ(init->signals & TASK_SIG_CHILD, 0)
        << "grandparent should not receive TASK_SIG_CHILD";
}

TEST_F(TaskManagerTest, destroy_task__without_init_fails)
{
    destroy_taskMgr();  // simulate uninitialized state
    EXPECT_EQ(destroy_task(1), -3) << "system not initialized";
}

TEST_F(TaskManagerTest, destroy_task__pid_0_fails)
{
    EXPECT_EQ(destroy_task(0), -1) << "task 0 can not be destroyed";
}

TEST_F(TaskManagerTest, destroy_task__existing_task)
{
    auto pid = create_task(0, 0, 0);
    EXPECT_EQ(destroy_task(pid), 0) << "task can be destroyed";
    EXPECT_EQ(GetTask(pid), nullptr) << "destroyed task must not be findable by PID";
}

TEST_F(TaskManagerTest, destroy_task__nonexistent_task)
{
    EXPECT_EQ(destroy_task(5), -1);
    EXPECT_EQ(destroy_task(9999), -1);
}

TEST_F(TaskManagerTest, destroy_task__zombie)
{
    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    EXPECT_EQ(terminate_task(pid, 11), 0);
    EXPECT_EQ(destroy_task(pid), 0) << "zombie can be reaped by destroy_task";
    EXPECT_EQ(GetTask(pid), nullptr);
}

TEST_F(TaskManagerTest, destroy_task__does_not_signal_parent)
{
    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    EXPECT_EQ(destroy_task(pid), 0);

    auto *init = GetTask(0);
    ASSERT_NE(init, nullptr);
    EXPECT_EQ(init->signals, 0)
        << "forced destroy should not send TASK_SIG_CHILD";
}

TEST_F(TaskManagerTest, send_signal__existing_task)
{
    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    EXPECT_EQ(send_signal(pid, TASK_SIG_TERM), 0);

    auto *task = GetTask(pid);
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->signals & TASK_SIG_TERM, TASK_SIG_TERM);
    EXPECT_EQ(task->state, TASK_STATE_READY) << "send_signal should not terminate the task";
}

TEST_F(TaskManagerTest, send_signal__accumulates_bits)
{
    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    EXPECT_EQ(send_signal(pid, TASK_SIG_TERM), 0);
    EXPECT_EQ(send_signal(pid, TASK_SIG_CHILD), 0);

    auto *task = GetTask(pid);
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->signals & TASK_SIG_TERM, TASK_SIG_TERM);
    EXPECT_EQ(task->signals & TASK_SIG_CHILD, TASK_SIG_CHILD);
}

TEST_F(TaskManagerTest, send_signal__task_not_found)
{
    EXPECT_EQ(send_signal(1, TASK_SIG_TERM), -1);
}

TEST_F(TaskManagerTest, send_signal__zombie_fails)
{
    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    EXPECT_EQ(terminate_task(pid, 0), 0);
    EXPECT_EQ(send_signal(pid, TASK_SIG_TERM), -1);
}

TEST_F(TaskManagerTest, send_signal__without_init_fails)
{
    destroy_taskMgr();
    EXPECT_EQ(send_signal(0, TASK_SIG_TERM), -3);
}

TEST_F(TaskManagerTest, block_task__existing_task)
{
    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    EXPECT_EQ(block_task(pid), 0);

    auto *task = GetTask(pid);
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->state, TASK_STATE_BLOCKED);
}

TEST_F(TaskManagerTest, unblock_task__existing_task)
{
    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    EXPECT_EQ(block_task(pid), 0);
    EXPECT_EQ(unblock_task(pid), 0);

    auto *task = GetTask(pid);
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->state, TASK_STATE_READY);
}

TEST_F(TaskManagerTest, block_task__task_not_found)
{
    EXPECT_EQ(block_task(1), -1);
    EXPECT_EQ(unblock_task(1), -1);
}

TEST_F(TaskManagerTest, block_task__zombie_fails)
{
    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    EXPECT_EQ(terminate_task(pid, 0), 0);
    EXPECT_EQ(block_task(pid), -1);
    EXPECT_EQ(unblock_task(pid), -1);
}

TEST_F(TaskManagerTest, block_task__without_init_fails)
{
    destroy_taskMgr();
    EXPECT_EQ(block_task(0), -3);
    EXPECT_EQ(unblock_task(0), -3);
}

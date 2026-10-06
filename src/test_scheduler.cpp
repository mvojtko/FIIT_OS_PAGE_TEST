#include "gtest/gtest.h"
#include "debug.h"
#include "test_common.h"

extern "C" {
#include "scheduler.h"
}

class SchedulerTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        memset(tasks, 0xcd, sizeof(tasks));
        task_mgr.tasks = tasks + 1;
        task_mgr.size = 16;
        ASSERT_EQ(init_taskMgr(&task_mgr), 0);
        CheckOverflow();
    }

    void TearDown() override
    {
        CheckOverflow();
        destroy_scheduler();
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

    tScheduler MakeScheduler(tSchedulingAlgorithm algorithm) const
    {
        return tScheduler{
            .current_task = static_cast<tPid>(-1),
            .task_runtime = -1,
            .total_runtime = -1,
            .scheduling_algorithm = algorithm,
        };
    }

    void VisualizeTasks(const tScheduler &scheduler) const
    {
        for (uint i = 0; i < task_mgr.size; i++)
        {
            auto task = task_mgr.tasks[i];
            if (task.pid == -1) continue;

            dprintf("tasks[%d] u-%d p-%d t-%d/%d %0.2f%% vt-%d/%d %0.2f%%\n",
                task.pid, task.uid, task.priority,
                task.runtime, scheduler.total_runtime, (float)task.runtime * 100 / (float)scheduler.total_runtime,
                task.runtime, scheduler.total_runtime - task.start_time, (float)task.runtime * 100 / (float)(scheduler.total_runtime - task.start_time));
        }
    }

    static int TicketCount(uint8_t priority)
    {
        return priority + 1;
    }

    void ExpectFairUserBalance(int uid1_hits, int uid2_hits, int ticks) const
    {
        EXPECT_EQ(uid1_hits + uid2_hits, ticks);
        EXPECT_NEAR(uid1_hits, uid2_hits, ticks / 10)
            << "each user should receive a fair share of the CPU";
    }

    void ExpectTaskRuntimeNear(tPid pid, int expected, int tolerance, const char *label) const
    {
        const tTaskStruct *task = GetTask(pid);
        ASSERT_NE(task, nullptr) << label;
        EXPECT_NEAR(task->runtime, expected, tolerance) << label;
    }

    void RunScheduler(int ticks)
    {
        for (int i = 0; i < ticks; i++)
        {
            schedule();
        }
    }

    tTaskStruct tasks[18];
    tTaskMgr task_mgr;
};

TEST(SchedulerTest_Init, init_scheduler__nullptr_does_not_enable_schedule)
{
    init_scheduler(nullptr);
    EXPECT_EQ(schedule(), -1) << "schedule should fail without a valid scheduler";
    destroy_scheduler();
}

TEST_F(SchedulerTest, init_scheduler__clears_runtime_and_selects_task_0)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    EXPECT_EQ(scheduler.current_task, -1);
    EXPECT_EQ(scheduler.task_runtime, 0);
    EXPECT_EQ(scheduler.total_runtime, -1);
    EXPECT_EQ(scheduler.scheduling_algorithm, SCHEDULING_ALGORITHM_ROUND_ROBIN);
}

TEST_F(SchedulerTest, schedule__without_init_fails)
{
    EXPECT_EQ(schedule(), -1) << "system not initialized";
}

TEST_F(SchedulerTest, schedule__without_init_taskMgr_fails)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);
    destroy_taskMgr();
    EXPECT_EQ(schedule(), -1) << "system not initialized";
}

TEST_F(SchedulerTest, schedule__after_destroy_fails)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);
    destroy_scheduler();
    EXPECT_EQ(schedule(), -1) << "destroyed scheduler must not schedule";
}

TEST_F(SchedulerTest, schedule__only_init_task_keeps_running)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    EXPECT_EQ(schedule(), 0);
    EXPECT_EQ(scheduler.current_task, 0);
    EXPECT_EQ(scheduler.total_runtime, 0);
    EXPECT_EQ(scheduler.task_runtime, 0);

    EXPECT_EQ(schedule(), 0);
    EXPECT_EQ(scheduler.current_task, 0);
    EXPECT_EQ(scheduler.total_runtime, 1);
    EXPECT_EQ(scheduler.task_runtime, 1);
}

TEST_F(SchedulerTest, schedule__increments_total_runtime_every_tick)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    for (int i = 0; i <= 8; i++)
    {
        schedule();
        EXPECT_EQ(scheduler.total_runtime, i);
        EXPECT_EQ(scheduler.task_runtime, i);
    }
}

TEST_F(SchedulerTest, schedule__returns_minus_one_when_no_ready_task)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);
    ASSERT_EQ(block_task(0), 0);

    EXPECT_EQ(schedule(), -1) << "no READY task is available";
    EXPECT_EQ(scheduler.total_runtime, 0) << "total_runtime still advances";
    EXPECT_EQ(schedule(), -1) << "no READY task is available";
    EXPECT_EQ(scheduler.total_runtime, 1) << "total_runtime still advances";
}

TEST_F(SchedulerTest, schedule__skips_blocked_task)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    ASSERT_EQ(block_task(0), 0);

    for (int i = 0; i < 12; i++)
    {
        EXPECT_EQ(schedule(), pid) << "blocked task 0 must not be selected";
    }
}

TEST_F(SchedulerTest, schedule__skips_zombie_task)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    ASSERT_EQ(terminate_task(pid, 1), 0);

    for (int i = 0; i < 8; i++)
    {
        EXPECT_EQ(schedule(), 0) << "zombie must not be scheduled";
    }
}

TEST_F(SchedulerTest, schedule__skips_destroyed_task)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    ASSERT_EQ(destroy_task(pid), 0);

    for (int i = 0; i < 8; i++)
    {
        EXPECT_EQ(schedule(), 0) << "destroyed task must not be scheduled";
    }
}

TEST_F(SchedulerTest, schedule__unblocked_task_becomes_runnable)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    ASSERT_EQ(block_task(pid), 0);

    for (int i = 0; i < 6; i++)
    {
        EXPECT_EQ(schedule(), 0);
    }

    ASSERT_EQ(unblock_task(pid), 0);
    bool seen = false;
    for (int i = 0; i < 32; i++)
    {
        if (schedule() == pid)
        {
            seen = true;
            break;
        }
    }
    EXPECT_TRUE(seen) << "unblocked task should be eligible to run";
}

TEST_F(SchedulerTest, schedule__adds_slice_runtime_to_task_on_switch)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    ASSERT_EQ(block_task(0), 0);

    // first schedule does nothing as no work has been done yet
    tPid previous = schedule();
    ASSERT_EQ(previous, pid);
    int32_t slice = scheduler.task_runtime;
    ASSERT_EQ(slice, 0);

    ASSERT_EQ(block_task(pid), 0);
    ASSERT_EQ(unblock_task(0), 0);

    tPid next = schedule();
    EXPECT_EQ(next, 0);
    EXPECT_NE(next, previous);

    auto *old_task = GetTask(pid);
    ASSERT_NE(old_task, nullptr);
    EXPECT_EQ(old_task->runtime, slice + 1)
        << "departing task must receive the current-cycle runtime";
}

TEST_F(SchedulerTest, schedule__round_robin_shares_time_without_priority)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    auto low = create_task(1, 0, 0);
    auto high = create_task(15, 0, 0);
    ASSERT_GT(low, 0);
    ASSERT_GT(high, 0);
    ASSERT_EQ(block_task(0), 0);

    const int ticks = 201;
    int low_hits = 0;
    int high_hits = 0;
    for (int i = 0; i < ticks; i++)
    {
        tPid pid = schedule();
        if (pid == low)
        {
            low_hits++;
        }
        else if (pid == high)
        {
            high_hits++;
        }
        else
        {
            ADD_FAILURE() << "unexpected pid " << pid;
        }
    }

    EXPECT_EQ(low_hits + high_hits, ticks);
    EXPECT_NEAR(low_hits, high_hits, 2)
        << "round robin must ignore priority";
    VisualizeTasks(scheduler);
}

TEST_F(SchedulerTest, schedule__round_robin_late_arrival_does_not_catch_up)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    auto first = create_task(3, 0, 0);
    auto second = create_task(12, 0, 0);
    ASSERT_GT(first, 0);
    ASSERT_GT(second, 0);
    ASSERT_EQ(block_task(0), 0);

    const int warmup_ticks = 400;
    const int post_arrival_ticks = 300;
    RunScheduler(warmup_ticks);

    const int first_before = GetTask(first)->runtime;
    const int second_before = GetTask(second)->runtime;
    ASSERT_GT(first_before, 0);
    ASSERT_GT(second_before, 0);

    auto late = create_task(15, 0, 0);
    ASSERT_GT(late, 0);

    RunScheduler(post_arrival_ticks);

    const int late_runtime = GetTask(late)->runtime;
    EXPECT_GT(late_runtime, 0) << "late arrival is included in the rotation";
    EXPECT_LT(late_runtime, first_before)
        << "round robin does not repay CPU missed before the task existed";
    EXPECT_LT(late_runtime, second_before)
        << "round robin does not repay CPU missed before the task existed";
    EXPECT_LT(late_runtime, GetTask(first)->runtime);
    EXPECT_LT(late_runtime, GetTask(second)->runtime);
    VisualizeTasks(scheduler);
}

TEST_F(SchedulerTest, schedule__round_robin_with_priority_prefers_higher_priority)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN_WITH_PRIORITY);
    init_scheduler(&scheduler);

    auto low = create_task(0, 0, 0);
    auto high = create_task(15, 0, 0);
    ASSERT_GT(low, 0);
    ASSERT_GT(high, 0);
    ASSERT_EQ(block_task(0), 0);

    const int ticks = 1701;
    int low_hits = 0;
    int high_hits = 0;
    for (int i = 0; i < ticks; i++)
    {
        tPid pid = schedule();
        if (pid == low)
        {
            low_hits++;
        }
        else if (pid == high)
        {
            high_hits++;
        }
    }

    EXPECT_EQ(low_hits + high_hits, ticks);
    EXPECT_GT(high_hits, low_hits)
        << "higher priority should receive a longer runtime";
    VisualizeTasks(scheduler);
}

TEST_F(SchedulerTest, schedule__round_robin_priority_late_arrival_keeps_quantum)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN_WITH_PRIORITY);
    init_scheduler(&scheduler);

    auto low = create_task(0, 0, 0);
    auto high = create_task(15, 0, 0);
    ASSERT_GT(low, 0);
    ASSERT_GT(high, 0);
    ASSERT_EQ(block_task(0), 0);

    const int warmup_ticks = 401;
    const int post_arrival_ticks = 400;
    RunScheduler(warmup_ticks);

    const int high_before = GetTask(high)->runtime;
    ASSERT_GT(high_before, GetTask(low)->runtime);

    auto late_high = create_task(15, 0, 0);
    ASSERT_GT(late_high, 0);

    int low_post = 0;
    int high_post = 0;
    int late_high_post = 0;
    for (int i = 0; i < post_arrival_ticks; i++)
    {
        tPid pid = schedule();
        if (pid == low)
        {
            low_post++;
        }
        else if (pid == high)
        {
            high_post++;
        }
        else if (pid == late_high)
        {
            late_high_post++;
        }
    }

    EXPECT_EQ(low_post + high_post + late_high_post, post_arrival_ticks);
    EXPECT_GT(late_high_post, low_post)
        << "a late high-priority task still receives a longer quantum";
    EXPECT_GT(high_post, low_post)
        << "existing high-priority task keeps its longer quantum";
    EXPECT_LT(GetTask(late_high)->runtime, high_before)
        << "priority round robin does not catch the new task up to past runtime";
    VisualizeTasks(scheduler);
}

TEST_F(SchedulerTest, schedule__random_with_priority_prefers_higher_priority)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_RANDOM_WITH_PRIORITY);
    init_scheduler(&scheduler);

    auto low = create_task(0, 0, 0);
    auto high = create_task(15, 0, 0);
    ASSERT_GT(low, 0);
    ASSERT_GT(high, 0);
    ASSERT_EQ(block_task(0), 0);

    const int ticks = 4001;
    int low_hits = 0;
    int high_hits = 0;
    for (int i = 0; i < ticks; i++)
    {
        tPid pid = schedule();
        if (pid == low)
        {
            low_hits++;
        }
        else if (pid == high)
        {
            high_hits++;
        }
    }

    EXPECT_EQ(low_hits + high_hits, ticks);
    EXPECT_GT(high_hits, low_hits * 2)
        << "lottery tickets should favour the higher-priority task";
    VisualizeTasks(scheduler);
}

TEST_F(SchedulerTest, schedule__random_priority_late_arrival_uses_tickets)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_RANDOM_WITH_PRIORITY);
    init_scheduler(&scheduler);

    auto low = create_task(0, 0, 0);
    auto high = create_task(15, 0, 0);
    ASSERT_GT(low, 0);
    ASSERT_GT(high, 0);
    ASSERT_EQ(block_task(0), 0);

    const int warmup_ticks = 400;
    const int post_arrival_ticks = 4000;
    RunScheduler(warmup_ticks);

    const int high_before = GetTask(high)->runtime;
    ASSERT_GT(high_before, 0);

    auto late_high = create_task(15, 0, 0);
    ASSERT_GT(late_high, 0);

    int low_post = 0;
    int high_post = 0;
    int late_high_post = 0;
    for (int i = 0; i < post_arrival_ticks; i++)
    {
        tPid pid = schedule();
        if (pid == low)
        {
            low_post++;
        }
        else if (pid == high)
        {
            high_post++;
        }
        else if (pid == late_high)
        {
            late_high_post++;
        }
    }

    EXPECT_EQ(low_post + high_post + late_high_post, post_arrival_ticks);
    EXPECT_GT(late_high_post, low_post * 2)
        << "a late high-priority task is drawn by its tickets, not by missed runtime";
    EXPECT_GT(high_post, low_post)
        << "existing high-priority task keeps its ticket advantage";
    EXPECT_LT(GetTask(late_high)->runtime, GetTask(high)->runtime)
        << "lottery does not catch a late task up to an incumbent with the same priority";
    VisualizeTasks(scheduler);
}

TEST_F(SchedulerTest, schedule__fair_share_balances_users)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_FAIR_SHARE_WITH_PRIORITY);
    init_scheduler(&scheduler);

    auto user_a = create_task(8, 0, 1);
    auto user_b1 = create_task(8, 0, 2);
    auto user_b2 = create_task(8, 0, 2);
    auto user_b3 = create_task(8, 0, 2);
    ASSERT_GT(user_a, 0);
    ASSERT_GT(user_b1, 0);
    ASSERT_GT(user_b2, 0);
    ASSERT_GT(user_b3, 0);
    ASSERT_EQ(block_task(0), 0);

    const int ticks = 2401;
    int uid1_hits = 0;
    int uid2_hits = 0;
    for (int i = 0; i < ticks; i++)
    {
        tPid pid = schedule();
        const tTaskStruct *task = GetTask(pid);
        ASSERT_NE(task, nullptr);
        if (task->uid == 1)
        {
            uid1_hits++;
        }
        else if (task->uid == 2)
        {
            uid2_hits++;
        }
    }

    ExpectFairUserBalance(uid1_hits, uid2_hits, ticks);

    const int per_task = uid2_hits / 3;
    ExpectTaskRuntimeNear(user_a, uid1_hits, ticks / 10, "user 1 task should get roughly half of CPU");
    ExpectTaskRuntimeNear(user_b1, per_task, ticks / 15, "equal-priority tasks of user 2 should share CPU evenly");
    ExpectTaskRuntimeNear(user_b2, per_task, ticks / 15, "equal-priority tasks of user 2 should share CPU evenly");
    ExpectTaskRuntimeNear(user_b3, per_task, ticks / 15, "equal-priority tasks of user 2 should share CPU evenly");
    EXPECT_GT(GetTask(user_b1)->runtime, 0) << "no task of user 2 should be starved";
    EXPECT_GT(GetTask(user_b2)->runtime, 0) << "no task of user 2 should be starved";
    EXPECT_GT(GetTask(user_b3)->runtime, 0) << "no task of user 2 should be starved";
    VisualizeTasks(scheduler);
}

TEST_F(SchedulerTest, schedule__fair_share_balances_users_with_priority)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_FAIR_SHARE_WITH_PRIORITY);
    init_scheduler(&scheduler);

    auto user_a = create_task(8, 0, 1);
    auto user_b1 = create_task(4, 0, 2);
    auto user_b2 = create_task(8, 0, 2);
    auto user_b3 = create_task(12, 0, 2);
    ASSERT_GT(user_a, 0);
    ASSERT_GT(user_b1, 0);
    ASSERT_GT(user_b2, 0);
    ASSERT_GT(user_b3, 0);
    ASSERT_EQ(block_task(0), 0);

    const int ticks = 2401;
    int uid1_hits = 0;
    int uid2_hits = 0;
    int b1_hits = 0;
    int b2_hits = 0;
    int b3_hits = 0;
    for (int i = 0; i < ticks; i++)
    {
        tPid pid = schedule();
        const tTaskStruct *task = GetTask(pid);
        ASSERT_NE(task, nullptr);
        if (task->uid == 1)
        {
            uid1_hits++;
        }
        else if (task->uid == 2)
        {
            uid2_hits++;
            if (pid == user_b1)
            {
                b1_hits++;
            }
            else if (pid == user_b2)
            {
                b2_hits++;
            }
            else if (pid == user_b3)
            {
                b3_hits++;
            }
        }
    }

    ExpectFairUserBalance(uid1_hits, uid2_hits, ticks);

    const int tickets_b1 = TicketCount(4);
    const int tickets_b2 = TicketCount(8);
    const int tickets_b3 = TicketCount(12);
    const int tickets_total = tickets_b1 + tickets_b2 + tickets_b3;

    ExpectTaskRuntimeNear(user_a, uid1_hits, ticks / 10, "user 1 task should get roughly half of CPU");
    ExpectTaskRuntimeNear(user_b1, uid2_hits * tickets_b1 / tickets_total, ticks / 15,
                          "priority 4 task should get 5/27 of user 2 share");
    ExpectTaskRuntimeNear(user_b2, uid2_hits * tickets_b2 / tickets_total, ticks / 15,
                          "priority 8 task should get 9/27 of user 2 share");
    ExpectTaskRuntimeNear(user_b3, uid2_hits * tickets_b3 / tickets_total, ticks / 15,
                          "priority 12 task should get 13/27 of user 2 share");
    EXPECT_GT(b3_hits, b2_hits) << "higher priority task should run more within user 2";
    EXPECT_GT(b2_hits, b1_hits) << "higher priority task should run more within user 2";
    EXPECT_GT(GetTask(user_b1)->runtime, 0) << "no task of user 2 should be starved";
    VisualizeTasks(scheduler);
}

TEST_F(SchedulerTest, schedule__fair_share_uses_priority_within_user)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_FAIR_SHARE_WITH_PRIORITY);
    init_scheduler(&scheduler);

    auto low = create_task(1, 0, 5);
    auto high = create_task(15, 0, 5);
    ASSERT_GT(low, 0);
    ASSERT_GT(high, 0);
    ASSERT_EQ(block_task(0), 0);

    const int ticks = 1600;
    int low_hits = 0;
    int high_hits = 0;
    for (int i = 0; i < ticks; i++)
    {
        tPid pid = schedule();
        if (pid == low)
        {
            low_hits++;
        }
        else if (pid == high)
        {
            high_hits++;
        }
    }

    EXPECT_EQ(low_hits + high_hits, ticks);

    const int low_tickets = TicketCount(1);
    const int high_tickets = TicketCount(15);
    const int tickets_total = low_tickets + high_tickets;

    ExpectTaskRuntimeNear(low, ticks * low_tickets / tickets_total, ticks / 15,
                          "low-priority task should get 2/18 of CPU");
    ExpectTaskRuntimeNear(high, ticks * high_tickets / tickets_total, ticks / 15,
                          "high-priority task should get 16/18 of CPU");
    EXPECT_GT(high_hits, low_hits * 2)
        << "within one user, higher priority should run noticeably longer";
    VisualizeTasks(scheduler);
}

TEST_F(SchedulerTest, schedule__fair_share_balances_late_arrival)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_FAIR_SHARE_WITH_PRIORITY);
    init_scheduler(&scheduler);

    auto user_a = create_task(8, 0, 1);
    auto user_b1 = create_task(8, 0, 2);
    ASSERT_GT(user_a, 0);
    ASSERT_GT(user_b1, 0);
    ASSERT_EQ(block_task(0), 0);

    const int warmup_ticks = 400;
    const int post_arrival_ticks = 2400;
    RunScheduler(warmup_ticks);

    const int b1_before = GetTask(user_b1)->runtime;
    ASSERT_GT(b1_before, 0);

    auto user_b2 = create_task(8, 0, 2);
    ASSERT_GT(user_b2, 0);
    EXPECT_EQ(GetTask(user_b2)->start_time, scheduler.total_runtime);

    int uid1_post = 0;
    int uid2_post = 0;
    int b1_post = 0;
    int b2_post = 0;
    for (int i = 0; i < post_arrival_ticks; i++)
    {
        tPid pid = schedule();
        const tTaskStruct *task = GetTask(pid);
        ASSERT_NE(task, nullptr);
        if (task->uid == 1)
        {
            uid1_post++;
        }
        else if (task->uid == 2)
        {
            uid2_post++;
            if (pid == user_b1)
            {
                b1_post++;
            }
            else if (pid == user_b2)
            {
                b2_post++;
            }
        }
    }

    ExpectFairUserBalance(uid1_post, uid2_post, post_arrival_ticks);
    EXPECT_NEAR(b1_post, uid2_post / 2, post_arrival_ticks / 10)
        << "after arrival, equal-priority tasks should split only future CPU";
    EXPECT_NEAR(b2_post, uid2_post / 2, post_arrival_ticks / 10)
        << "late arrival should receive its future share without catching up the past";
    EXPECT_GT(GetTask(user_b1)->runtime, GetTask(user_b2)->runtime + b1_before / 2)
        << "time before start_time must not be repaid to the new task";
    EXPECT_GT(b1_post, 0) << "incumbent must keep receiving CPU after the new task arrives";
    VisualizeTasks(scheduler);
}

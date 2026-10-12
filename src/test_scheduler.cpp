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
            .seed = 1,
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

TEST(SchedulerTest_Init, get_total_runtime__without_scheduler_returns_zero)
{
    EXPECT_EQ(get_total_runtime(), 0);
}

TEST_F(SchedulerTest, get_total_runtime__after_init_returns_zero_before_schedule)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    EXPECT_EQ(get_total_runtime(), 0);
    EXPECT_EQ(scheduler.total_runtime, -1) << "internal sentinel before the first tick";
}

TEST_F(SchedulerTest, get_total_runtime__grows_on_each_schedule_without_runnable_tasks)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);
    ASSERT_EQ(block_task(0), 0);

    EXPECT_EQ(get_total_runtime(), 0);
    for (int tick = 0; tick < 8; tick++)
    {
        EXPECT_EQ(schedule(), -1) << "no READY task is available";
        EXPECT_EQ(scheduler.total_runtime, tick);
        EXPECT_EQ(get_total_runtime(), tick) << "reported runtime must follow each tick";
    }
}

TEST_F(SchedulerTest, get_total_runtime__grows_on_each_schedule_with_running_task)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    EXPECT_EQ(get_total_runtime(), 0);
    for (int tick = 0; tick < 8; tick++)
    {
        EXPECT_EQ(schedule(), 0);
        EXPECT_EQ(scheduler.total_runtime, tick);
        EXPECT_EQ(get_total_runtime(), tick);
    }
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
    EXPECT_EQ(GetTask(0)->runtime, 0);

    EXPECT_EQ(schedule(), 0);
    EXPECT_EQ(scheduler.current_task, 0);
    EXPECT_EQ(scheduler.total_runtime, 1);
    EXPECT_EQ(scheduler.task_runtime, 1);
    EXPECT_EQ(GetTask(0)->runtime, 1);
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
        EXPECT_EQ(GetTask(0)->runtime, i);
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

TEST_F(SchedulerTest, schedule__sig_child_reaps_only_direct_child_zombies)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    auto parent_a = create_task(0, 0, 0);
    auto child_a = create_task(0, parent_a, 0);
    auto parent_b = create_task(0, 0, 0);
    auto child_b = create_task(0, parent_b, 0);
    ASSERT_GT(parent_a, 0);
    ASSERT_GT(child_a, 0);
    ASSERT_GT(parent_b, 0);
    ASSERT_GT(child_b, 0);

    ASSERT_EQ(terminate_task(child_a, 3), 0);
    ASSERT_EQ(terminate_task(child_b, 4), 0);

    const tTaskStruct *zombie_a = GetTask(child_a);
    const tTaskStruct *zombie_b = GetTask(child_b);
    ASSERT_NE(zombie_a, nullptr);
    ASSERT_NE(zombie_b, nullptr);
    EXPECT_EQ(zombie_a->state, TASK_STATE_TERMINATED);
    EXPECT_EQ(zombie_b->state, TASK_STATE_TERMINATED);
    EXPECT_EQ(GetTask(parent_a)->signals & TASK_SIG_CHILD, TASK_SIG_CHILD);
    EXPECT_EQ(GetTask(parent_b)->signals & TASK_SIG_CHILD, TASK_SIG_CHILD);

    ASSERT_EQ(block_task(0), 0);
    ASSERT_EQ(block_task(parent_b), 0);

    EXPECT_EQ(schedule(), parent_a) << "parent_a should run and handle TASK_SIG_CHILD";

    EXPECT_EQ(GetTask(child_a), nullptr) << "direct child zombie of parent_a must be reaped";
    zombie_b = GetTask(child_b);
    ASSERT_NE(zombie_b, nullptr);
    EXPECT_EQ(zombie_b->state, TASK_STATE_TERMINATED);
    EXPECT_EQ(zombie_b->exit_code, 4) << "unrelated zombie must remain until its parent reaps it";
    EXPECT_EQ(GetTask(parent_a)->signals & TASK_SIG_CHILD, 0);
    EXPECT_EQ(GetTask(parent_b)->signals & TASK_SIG_CHILD, TASK_SIG_CHILD)
        << "parent_b must keep TASK_SIG_CHILD until it is scheduled";
}

TEST_F(SchedulerTest, schedule__sig_term_terminates_task_when_scheduled)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    auto worker = create_task(0, 0, 0);
    ASSERT_GT(worker, 0);
    ASSERT_EQ(send_signal(worker, TASK_SIG_TERM), 0);
    ASSERT_EQ(block_task(0), 0);

    EXPECT_EQ(schedule(), -1) << "worker becomes a zombie while handling TASK_SIG_TERM";

    const tTaskStruct *task = GetTask(worker);
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->state, TASK_STATE_TERMINATED);
    EXPECT_EQ(task->exit_code, 0);
    EXPECT_EQ(task->signals & TASK_SIG_TERM, 0);
    EXPECT_EQ(GetTask(0)->signals & TASK_SIG_CHILD, TASK_SIG_CHILD)
        << "parent should be notified about the child exit";
}

TEST_F(SchedulerTest, schedule__sig_term_does_not_terminate_init_task)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    ASSERT_EQ(send_signal(0, TASK_SIG_TERM), 0);

    EXPECT_EQ(schedule(), 0);
    const tTaskStruct *init = GetTask(0);
    ASSERT_NE(init, nullptr);
    EXPECT_EQ(init->state, TASK_STATE_READY);
    EXPECT_EQ(init->signals & TASK_SIG_TERM, 0);
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

TEST_F(SchedulerTest, schedule__round_robin_handles_block_destroy_and_add)
{
    tScheduler scheduler = MakeScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN);
    init_scheduler(&scheduler);

    auto task_a = create_task(0, 0, 0);
    auto task_b = create_task(0, 0, 0);
    ASSERT_GT(task_a, 0);
    ASSERT_GT(task_b, 0);
    ASSERT_EQ(block_task(0), 0);

    const int pair_ticks = 120;
    int a_hits = 0;
    int b_hits = 0;
    for (int i = 0; i < pair_ticks; i++)
    {
        tPid pid = schedule();
        if (pid == task_a)
        {
            a_hits++;
        }
        else if (pid == task_b)
        {
            b_hits++;
        }
        else
        {
            ADD_FAILURE() << "unexpected pid " << pid;
        }
    }
    EXPECT_EQ(a_hits + b_hits, pair_ticks);
    EXPECT_NEAR(a_hits, b_hits, 2) << "two ready tasks should alternate";

    ASSERT_EQ(block_task(task_a), 0);
    const int solo_ticks = 50;
    for (int i = 0; i < solo_ticks; i++)
    {
        EXPECT_EQ(schedule(), task_b) << "only task_b is ready while task_a is blocked";
    }
    const int b_runtime_before_add = GetTask(task_b)->runtime;
    const int a_runtime_while_blocked = GetTask(task_a)->runtime;

    auto task_c = create_task(0, 0, 0);
    ASSERT_GT(task_c, 0);
    const int duo_ticks = 80;
    int b_duo = 0;
    int c_duo = 0;
    for (int i = 0; i < duo_ticks; i++)
    {
        tPid pid = schedule();
        if (pid == task_b)
        {
            b_duo++;
        }
        else if (pid == task_c)
        {
            c_duo++;
        }
        else
        {
            ADD_FAILURE() << "unexpected pid " << pid << " with task_a still blocked";
        }
    }
    EXPECT_EQ(b_duo + c_duo, duo_ticks);
    EXPECT_NEAR(b_duo, c_duo, 2) << "new task joins the rotation without catch-up debt";
    EXPECT_EQ(GetTask(task_a)->runtime, a_runtime_while_blocked)
        << "blocked task must not accumulate runtime";

    ASSERT_EQ(unblock_task(task_a), 0);
    const int trio_ticks = 90;
    int a_trio = 0;
    int b_trio = 0;
    int c_trio = 0;
    for (int i = 0; i < trio_ticks; i++)
    {
        tPid pid = schedule();
        if (pid == task_a)
        {
            a_trio++;
        }
        else if (pid == task_b)
        {
            b_trio++;
        }
        else if (pid == task_c)
        {
            c_trio++;
        }
        else
        {
            ADD_FAILURE() << "unexpected pid " << pid;
        }
    }
    EXPECT_EQ(a_trio + b_trio + c_trio, trio_ticks);
    EXPECT_NEAR(a_trio, b_trio, 2);
    EXPECT_NEAR(b_trio, c_trio, 2);
    EXPECT_NEAR(a_trio, c_trio, 2) << "unblocked task re-enters fair rotation";

    const int b_runtime_before_destroy = GetTask(task_b)->runtime;
    ASSERT_EQ(destroy_task(task_b), 0);
    const int after_destroy_ticks = 60;
    int a_post = 0;
    int c_post = 0;
    for (int i = 0; i < after_destroy_ticks; i++)
    {
        tPid pid = schedule();
        if (pid == task_a)
        {
            a_post++;
        }
        else if (pid == task_c)
        {
            c_post++;
        }
        else
        {
            ADD_FAILURE() << "unexpected pid " << pid;
        }
    }
    EXPECT_EQ(a_post + c_post, after_destroy_ticks);
    EXPECT_NEAR(a_post, c_post, 2) << "rotation continues among remaining ready tasks";
    EXPECT_EQ(GetTask(task_b), nullptr) << "destroyed task slot is cleared";
    EXPECT_EQ(b_runtime_before_destroy, b_runtime_before_add + b_duo + b_trio)
        << "destroyed task keeps runtime earned before removal";

    const int a_runtime_before_d = GetTask(task_a)->runtime;
    const int c_runtime_before_d = GetTask(task_c)->runtime;

    auto task_d = create_task(0, 0, 0);
    ASSERT_GT(task_d, 0);
    EXPECT_EQ(task_d, task_b) << "destroyed task's PID should be recycled for the new task";
    EXPECT_EQ(GetTask(task_d)->start_time, scheduler.total_runtime);

    const int trio_after_d_ticks = 90;
    int a_after_d = 0;
    int c_after_d = 0;
    int d_after_d = 0;
    for (int i = 0; i < trio_after_d_ticks; i++)
    {
        tPid pid = schedule();
        if (pid == task_a)
        {
            a_after_d++;
        }
        else if (pid == task_c)
        {
            c_after_d++;
        }
        else if (pid == task_d)
        {
            d_after_d++;
        }
        else
        {
            ADD_FAILURE() << "unexpected pid " << pid;
        }
    }
    EXPECT_EQ(a_after_d + c_after_d + d_after_d, trio_after_d_ticks);
    EXPECT_NEAR(a_after_d, c_after_d, 2);
    EXPECT_NEAR(c_after_d, d_after_d, 2);
    EXPECT_NEAR(a_after_d, d_after_d, 2) << "late task_d joins rotation among survivors";
    EXPECT_GT(d_after_d, 0);
    EXPECT_LT(GetTask(task_d)->runtime, a_runtime_before_d)
        << "round robin does not repay CPU missed before task_d existed";
    EXPECT_LT(GetTask(task_d)->runtime, c_runtime_before_d)
        << "round robin does not repay CPU missed before task_d existed";
    EXPECT_LT(GetTask(task_d)->runtime, GetTask(task_a)->runtime);
    EXPECT_LT(GetTask(task_d)->runtime, GetTask(task_c)->runtime);
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

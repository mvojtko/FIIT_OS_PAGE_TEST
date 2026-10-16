#include "test_common.h"

#include <memory>
#include <string>
#include <utility>

extern "C" {
#include "scheduler.h"
}

class SchedulerTest : public OsTestFixture
{
  protected:
    void SetUp() override
    {
        OsTestFixture::SetUp();
        InitScheduler();
    }

    void TearDown() override
    {
        CheckOverflow();
        destroy_scheduler();
        OsTestFixture::TearDown();
    }

    static std::unique_ptr<tScheduler> MakeScheduler(tSchedulingAlgorithm algorithm, uint32_t seed = 1)
    {
        return std::make_unique<tScheduler>(tScheduler{
            .current_task = static_cast<tPid>(-1),
            .task_runtime = -1,
            .total_runtime = -1,
            .seed = seed,
            .scheduling_algorithm = algorithm});
    }

    void InitScheduler(tSchedulingAlgorithm algorithm = SCHEDULING_ALGORITHM_ROUND_ROBIN,
                       uint32_t seed = 1)
    {
        scheduler_ = std::move(MakeScheduler(algorithm, seed));
        init_scheduler(scheduler_.get());
    }

    void BlockInitTask()
    {
        ASSERT_EQ(block_task(0), 0);
    }

    std::pair<tPid, tPid> AddLowHighTasks(uint8_t low_priority, uint8_t high_priority, uint8_t uid = 0)
    {
        tPid low = CreateTask(low_priority, 0, uid);
        tPid high = CreateTask(high_priority, 0, uid);
        BlockInitTask();
        return {low, high};
    }

    void RunSchedulerStrict(int ticks, tPid pid_a, tPid pid_b) const
    {
        for (int i = 0; i < ticks + 1; i++)
        {
            tPid pid = schedule();
            if (pid != pid_a && pid != pid_b)
            {
                ADD_FAILURE() << "unexpected pid " << pid;
            }
        }
    }

    void VisualizeTasks() const
    {
        for (uint8_t i = 0; i < task_mgr.size; i++)
        {
            auto task = task_mgr.tasks[i];
            if (task.pid == -1) continue;

            dprintf("tasks[%d] u-%d p-%d t-%d/%d %0.2f%%\n",
                task.pid, task.uid, task.priority,
                task.runtime, scheduler_->total_runtime,
                (float)task.runtime * 100 / (float)scheduler_->total_runtime);
        }
        dprintf("------------------------------------\n");
    }

    static int TicketCount(uint8_t priority)
    {
        return priority + 1;
    }

    void ExpectFairUserBalance(int uid1_hits, int uid2_hits, int ticks) const
    {
        EXPECT_EQ(uid1_hits + uid2_hits, ticks);
        EXPECT_NEAR(uid1_hits, uid2_hits, ticks / 100)
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
        for (int i = 0; i < ticks + 1; i++)
        {
            schedule();
        }
    }

    std::unique_ptr<tScheduler> scheduler_;
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

TEST(SchedulerTest_Init, schedule__without_init_fails)
{
    EXPECT_EQ(schedule(), -1) << "system not initialized";
}

TEST_F(SchedulerTest, get_total_runtime__after_init_returns_zero_before_schedule)
{
    EXPECT_EQ(get_total_runtime(), 0);
    EXPECT_EQ(scheduler_->total_runtime, -1) << "internal sentinel before the first tick";
}

TEST_F(SchedulerTest, get_total_runtime__grows_on_each_schedule_without_runnable_tasks)
{
    ASSERT_EQ(block_task(0), 0);

    EXPECT_EQ(get_total_runtime(), 0);
    for (int tick = 0; tick < 8; tick++)
    {
        EXPECT_EQ(schedule(), -1) << "no READY task is available";
        EXPECT_EQ(scheduler_->total_runtime, tick);
        EXPECT_EQ(get_total_runtime(), tick) << "reported runtime must follow each tick";
    }
}

TEST_F(SchedulerTest, get_total_runtime__grows_on_each_schedule_with_running_task)
{
    EXPECT_EQ(get_total_runtime(), 0);
    for (int tick = 0; tick < 8; tick++)
    {
        EXPECT_EQ(schedule(), 0);
        EXPECT_EQ(scheduler_->total_runtime, tick);
        EXPECT_EQ(get_total_runtime(), tick);
    }
}

TEST_F(SchedulerTest, init_scheduler__clears_runtime_and_selects_no_task)
{
    EXPECT_EQ(scheduler_->current_task, -1);
    EXPECT_EQ(scheduler_->task_runtime, 0);
    EXPECT_EQ(scheduler_->total_runtime, -1);
    EXPECT_EQ(scheduler_->seed, 1u);
    EXPECT_EQ(scheduler_->scheduling_algorithm, SCHEDULING_ALGORITHM_ROUND_ROBIN);
}

TEST_F(SchedulerTest, schedule__without_init_taskMgr_fails)
{
    destroy_taskMgr();
    EXPECT_EQ(schedule(), -1) << "system not initialized";
}

TEST_F(SchedulerTest, schedule__after_destroy_fails)
{
    destroy_scheduler();
    EXPECT_EQ(schedule(), -1) << "destroyed scheduler must not schedule";
}

TEST_F(SchedulerTest, get_total_runtime__after_destroy_returns_zero)
{
    ASSERT_EQ(schedule(), 0);
    ASSERT_EQ(schedule(), 0);
    EXPECT_EQ(get_total_runtime(), 1);
    destroy_scheduler();
    EXPECT_EQ(get_total_runtime(), 0);
}

TEST_F(SchedulerTest, schedule__only_init_task_keeps_running)
{
    EXPECT_EQ(schedule(), 0);
    EXPECT_EQ(scheduler_->current_task, 0);
    EXPECT_EQ(scheduler_->total_runtime, 0);
    EXPECT_EQ(scheduler_->task_runtime, 0);
    EXPECT_EQ(GetTask(0)->runtime, 0);

    EXPECT_EQ(schedule(), 0);
    EXPECT_EQ(scheduler_->current_task, 0);
    EXPECT_EQ(scheduler_->total_runtime, 1);
    EXPECT_EQ(scheduler_->task_runtime, 1);
    EXPECT_EQ(GetTask(0)->runtime, 1);
}

TEST_F(SchedulerTest, schedule__increments_total_runtime_every_tick)
{
    for (int i = 0; i <= 8; i++)
    {
        schedule();
        EXPECT_EQ(scheduler_->total_runtime, i);
        EXPECT_EQ(scheduler_->task_runtime, i);
        EXPECT_EQ(GetTask(0)->runtime, i);
    }
}

TEST_F(SchedulerTest, schedule__returns_minus_one_when_no_ready_task)
{
    ASSERT_EQ(block_task(0), 0);

    EXPECT_EQ(schedule(), -1) << "no READY task is available";
    EXPECT_EQ(scheduler_->total_runtime, 0) << "total_runtime still advances";
    EXPECT_EQ(schedule(), -1) << "no READY task is available";
    EXPECT_EQ(scheduler_->total_runtime, 1) << "total_runtime still advances";
}

TEST_F(SchedulerTest, schedule__skips_blocked_task)
{
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
    auto parent_a = CreateTask();
    auto child_a = CreateTask(0, parent_a);
    auto parent_b = CreateTask();
    auto child_b = CreateTask(0, parent_b);

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

TEST_F(SchedulerTest, schedule__sig_child_keeps_living_child)
{
    auto parent = CreateTask();
    auto child = CreateTask(0, parent);
    ASSERT_EQ(send_signal(parent, TASK_SIG_CHILD), 0);
    ASSERT_EQ(block_task(0), 0);
    ASSERT_EQ(block_task(child), 0);

    EXPECT_EQ(schedule(), parent);
    EXPECT_EQ(GetTask(parent)->signals & TASK_SIG_CHILD, 0);
    const tTaskStruct *living = GetTask(child);
    ASSERT_NE(living, nullptr);
    EXPECT_EQ(living->state, TASK_STATE_BLOCKED);
    EXPECT_EQ(living->pid, child);
}

TEST_F(SchedulerTest, schedule__sig_child_then_sig_term_reaps_before_exit)
{
    auto parent = CreateTask();
    auto child = CreateTask(0, parent);
    ASSERT_EQ(terminate_task(child, 5), 0);
    ASSERT_EQ(send_signal(parent, TASK_SIG_TERM), 0);
    ASSERT_EQ(block_task(0), 0);

    EXPECT_EQ(schedule(), -1) << "parent exits after reaping its zombie child";
    EXPECT_EQ(GetTask(child), nullptr) << "TASK_SIG_CHILD reaps the zombie before TASK_SIG_TERM";

    const tTaskStruct *zombie = GetTask(parent);
    ASSERT_NE(zombie, nullptr);
    EXPECT_EQ(zombie->state, TASK_STATE_TERMINATED);
    EXPECT_EQ(zombie->exit_code, 0);
    EXPECT_EQ(zombie->signals & (TASK_SIG_CHILD | TASK_SIG_TERM), 0);
    EXPECT_EQ(GetTask(0)->signals & TASK_SIG_CHILD, TASK_SIG_CHILD);
}

TEST_F(SchedulerTest, schedule__sig_term_terminates_task_when_scheduled)
{
    auto worker = CreateTask();
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
    ASSERT_EQ(send_signal(0, TASK_SIG_TERM), 0);

    EXPECT_EQ(schedule(), 0);
    const tTaskStruct *init = GetTask(0);
    ASSERT_NE(init, nullptr);
    EXPECT_EQ(init->state, TASK_STATE_READY);
    EXPECT_EQ(init->signals & TASK_SIG_TERM, 0);
}

TEST_F(SchedulerTest, schedule__sig_term_on_blocked_task_waits_until_unblocked)
{
    auto worker = CreateTask();
    ASSERT_EQ(block_task(worker), 0);
    ASSERT_EQ(send_signal(worker, TASK_SIG_TERM), 0);

    for (int i = 0; i < 4; i++)
    {
        EXPECT_EQ(schedule(), 0) << "a blocked task is not selected to handle TASK_SIG_TERM";
    }
    const tTaskStruct *blocked = GetTask(worker);
    ASSERT_NE(blocked, nullptr);
    EXPECT_EQ(blocked->state, TASK_STATE_BLOCKED);
    EXPECT_EQ(blocked->signals & TASK_SIG_TERM, TASK_SIG_TERM);

    ASSERT_EQ(block_task(0), 0);
    ASSERT_EQ(unblock_task(worker), 0);
    EXPECT_EQ(schedule(), -1) << "TASK_SIG_TERM is handled once the task is scheduled";

    const tTaskStruct *task = GetTask(worker);
    ASSERT_NE(task, nullptr) << "init is blocked, so the zombie is not reaped in this tick";
    EXPECT_EQ(task->state, TASK_STATE_TERMINATED);
    EXPECT_EQ(task->exit_code, 0);
    EXPECT_EQ(task->signals & TASK_SIG_TERM, 0);
    EXPECT_EQ(GetTask(0)->signals & TASK_SIG_CHILD, TASK_SIG_CHILD);
}

TEST_F(SchedulerTest, schedule__skips_destroyed_task)
{
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
    auto pid = create_task(0, 0, 0);
    ASSERT_GT(pid, 0);
    ASSERT_EQ(block_task(0), 0);

    // first schedule does nothing as no work has been done yet
    tPid previous = schedule();
    ASSERT_EQ(previous, pid);
    int32_t slice = scheduler_->task_runtime;
    ASSERT_EQ(slice, 0);

    ASSERT_EQ(block_task(pid), 0);
    ASSERT_EQ(unblock_task(0), 0);

    tPid next = schedule();
    EXPECT_EQ(next, 0);
    EXPECT_NE(next, previous);
    EXPECT_EQ(scheduler_->task_runtime, 0) << "task_runtime resets when the running task changes";

    auto *old_task = GetTask(pid);
    ASSERT_NE(old_task, nullptr);
    EXPECT_EQ(old_task->runtime, slice + 1)
        << "departing task must receive the current-cycle runtime";
}

TEST_F(SchedulerTest, schedule__round_robin_alternates_in_storage_order)
{
    auto pid = CreateTask();
    auto pid1 = CreateTask();
    const tPid expected[] = {0, pid, pid1, 0, pid, pid1, 0, pid, pid1};
    for (tPid want : expected)
    {
        EXPECT_EQ(schedule(), want);
    }
}

TEST_F(SchedulerTest, schedule__round_robin_alternates_in_storage_order_with_gaps)
{
    auto pid = CreateTask();
    auto pid1 = CreateTask();
    auto pid2 = CreateTask();
    destroy_task(pid1);
    const tPid expected[] = {0, pid, pid2, 0, pid, pid2, 0, pid, pid2};
    for (tPid want : expected)
    {
        EXPECT_EQ(schedule(), want);
    }
}

TEST_F(SchedulerTest, schedule__round_robin_alternates_in_storage_order_gap_after_some_run)
{
    auto pid = CreateTask();
    auto pid1 = CreateTask();
    auto pid2 = CreateTask();
    const tPid expected[] = {0, pid, pid1, pid2, 0, pid, pid1, pid2, 0, pid};
    for (tPid want : expected)
    {
        EXPECT_EQ(schedule(), want);
    }

    EXPECT_EQ(destroy_task(pid), 0);
    const tPid expected1[] = {pid1, pid2, 0, pid1, pid2, 0};
    for (tPid want : expected1)
    {
        EXPECT_EQ(schedule(), want);
    }
}

TEST_F(SchedulerTest, schedule__round_robin_shares_time_without_priority)
{
    auto [low, high] = AddLowHighTasks(1, 15);

    const int ticks = 200;
    RunSchedulerStrict(ticks, low, high);

    int low_hits = GetTask(low)->runtime;
    int high_hits = GetTask(high)->runtime;

    EXPECT_EQ(low_hits + high_hits, ticks);
    EXPECT_NEAR(low_hits, high_hits, 2)
        << "round robin must ignore priority";
    VisualizeTasks();
}

TEST_F(SchedulerTest, schedule__round_robin_late_arrival_does_not_catch_up)
{
    auto [first, second] = AddLowHighTasks(3, 12);

    const int warmup_ticks = 400;
    const int post_arrival_ticks = 299;
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
    VisualizeTasks();
}

TEST_F(SchedulerTest, schedule__round_robin_handles_block_destroy_and_create)
{
    auto task_a = CreateTask();
    auto task_b = CreateTask();
    BlockInitTask();

    const int pair_ticks = 120;
    RunSchedulerStrict(pair_ticks, task_a, task_b);
    auto a_hits = GetTask(task_a)->runtime;
    auto b_hits = GetTask(task_b)->runtime;
    EXPECT_EQ(a_hits + b_hits, pair_ticks);
    EXPECT_NEAR(a_hits, b_hits, 2) << "two ready tasks should alternate";

    ASSERT_EQ(block_task(task_a), 0);
    const int solo_ticks = 50;
    for (int i = 0; i < solo_ticks; i++)
    {
        EXPECT_EQ(schedule(), task_b) << "only task_b is ready while task_a is blocked";
    }
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

    const int a_runtime_before_d = GetTask(task_a)->runtime;
    const int c_runtime_before_d = GetTask(task_c)->runtime;

    auto task_d = create_task(0, 0, 0);
    ASSERT_GT(task_d, 0);
    EXPECT_EQ(task_d, task_b) << "destroyed task's PID should be recycled for the new task";
    EXPECT_EQ(GetTask(task_d)->start_time, scheduler_->total_runtime);

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
    VisualizeTasks();
}

TEST_F(SchedulerTest, schedule__round_robin_priority_runs_consecutive_quanta)
{
    InitScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN_WITH_PRIORITY);
    auto low = CreateTask(0);
    auto high = CreateTask(2);
    BlockInitTask();

    const tPid expected[] = {low, high, high, high, low, high, high, high, low};
    for (tPid want : expected)
    {
        EXPECT_EQ(schedule(), want);
    }
    EXPECT_EQ(scheduler_->task_runtime, 0) << "task_runtime resets when the quantum ends";
    EXPECT_EQ(GetTask(low)->runtime, 2);
    EXPECT_EQ(GetTask(high)->runtime, 6) << "priority 2 keeps the cpu for 3 consecutive ticks";
}

TEST_F(SchedulerTest, schedule__round_robin_priority_runs_consecutive_quanta_with_gaps)
{
    InitScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN_WITH_PRIORITY);
    auto low = CreateTask(0);
    auto gap = CreateTask(5);
    auto high = CreateTask(2);
    destroy_task(gap);

    const tPid expected[] = {0, low, high, high, high, 0, low, high, high, high, 0, low};
    for (tPid want : expected)
    {
        EXPECT_EQ(schedule(), want);
    }
    EXPECT_EQ(scheduler_->task_runtime, 0) << "task_runtime resets when the quantum ends";
    EXPECT_EQ(GetTask(low)->runtime, 2);
    EXPECT_EQ(GetTask(high)->runtime, 6) << "priority 2 keeps the cpu for 3 consecutive ticks";
}

TEST_F(SchedulerTest, schedule__round_robin_priority_alternates_in_storage_order_gap_after_some_run)
{
    InitScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN_WITH_PRIORITY);
    auto pid = CreateTask(0);
    auto pid1 = CreateTask(1);
    auto pid2 = CreateTask(2);
    const tPid expected[] = {0, pid, pid1, pid1, pid2, pid2, pid2, 0, pid, pid1, pid1, pid2, pid2, pid2, 0, pid};
    for (tPid want : expected)
    {
        EXPECT_EQ(schedule(), want);
    }

    EXPECT_EQ(destroy_task(pid), 0);
    const tPid expected1[] = {pid1, pid1, pid2, pid2, pid2, 0, pid1, pid1, pid2, pid2, pid2, 0};
    for (tPid want : expected1)
    {
        EXPECT_EQ(schedule(), want);
    }
}

TEST_F(SchedulerTest, schedule__round_robin_with_priority_prefers_higher_priority)
{
    InitScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN_WITH_PRIORITY);
    auto [low, high] = AddLowHighTasks(0, 15);

    const int ticks = 1700;
    RunScheduler(ticks);

    auto low_hits = GetTask(low)->runtime;
    auto high_hits = GetTask(high)->runtime;

    EXPECT_EQ(low_hits + high_hits, ticks);
    EXPECT_GT(high_hits, low_hits)
        << "higher priority should receive a longer runtime";
    EXPECT_NEAR(high_hits, low_hits * 16, 2)
        << "higher priority should receive a longer runtime";
    VisualizeTasks();
}

TEST_F(SchedulerTest, schedule__round_robin_priority_late_arrival_keeps_quantum)
{
    InitScheduler(SCHEDULING_ALGORITHM_ROUND_ROBIN_WITH_PRIORITY);
    auto [low, high] = AddLowHighTasks(0, 15);

    const int warmup_ticks = 400;
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
    EXPECT_NEAR(high_post, low_post * 16, 20)
        << "existing high-priority task keeps a 16 tick quantum";
    EXPECT_NEAR(late_high_post, high_post, 20)
        << "a late task with the same priority gets the same quantum, not past runtime";
    EXPECT_LT(GetTask(late_high)->runtime, high_before)
        << "priority round robin does not catch the new task up to past runtime";
    VisualizeTasks();
}

TEST_F(SchedulerTest, schedule__random_with_priority_prefers_higher_priority)
{
    InitScheduler(SCHEDULING_ALGORITHM_RANDOM_WITH_PRIORITY);
    tPid low = 0;
    tPid high = CreateTask(15, 0, 5);

    const int ticks = 17000;
    RunScheduler(ticks);

    auto low_hits = GetTask(low)->runtime;
    auto high_hits = GetTask(high)->runtime;

    EXPECT_EQ(low_hits + high_hits, ticks);
    EXPECT_NEAR(high_hits, low_hits * 16, ticks/17)
        << "lottery tickets should favour the higher-priority task";
    VisualizeTasks();
}

TEST_F(SchedulerTest, schedule__random_with_priority_prefers_higher_priority_reverse)
{
    InitScheduler(SCHEDULING_ALGORITHM_RANDOM_WITH_PRIORITY);
    tPid low = 0;
    tPid high = CreateTask(15, 0, 5);

    const int ticks = 17000;
    RunScheduler(ticks);

    auto low_hits = GetTask(low)->runtime;
    auto high_hits = GetTask(high)->runtime;

    EXPECT_EQ(low_hits + high_hits, ticks);
    EXPECT_NEAR(high_hits, low_hits * 16, ticks/17)
        << "lottery tickets should favour the higher-priority task";
    VisualizeTasks();
}

TEST_F(SchedulerTest, schedule__random_seed_changes_sequence)
{
    auto low = CreateTask(0);
    auto high = CreateTask(15);
    BlockInitTask();

    auto sample = [&](uint32_t seed) {
        InitScheduler(SCHEDULING_ALGORITHM_RANDOM_WITH_PRIORITY, seed);
        EXPECT_EQ(scheduler_->seed, seed);
        std::string sequence;
        for (int i = 0; i < 32; i++)
        {
            tPid pid = schedule();
            EXPECT_TRUE(pid == low || pid == high) << "unexpected pid " << pid;
            sequence.push_back(static_cast<char>(pid));
        }
        return sequence;
    };

    EXPECT_NE(sample(1), sample(2)) << "the lottery sequence must follow the scheduler seed";
}

TEST_F(SchedulerTest, schedule__random_priority_late_arrival_uses_tickets)
{
    InitScheduler(SCHEDULING_ALGORITHM_RANDOM_WITH_PRIORITY);
    tPid low = 0;
    tPid high = CreateTask(15, 0, 0);

    const int warmup_ticks = 400;
    const int post_arrival_ticks = 4000;
    RunScheduler(warmup_ticks);
    VisualizeTasks();

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
    EXPECT_NEAR(late_high_post, low_post * 16, post_arrival_ticks / 10)
        << "a late high-priority task is drawn by its tickets, not by missed runtime";
    EXPECT_NEAR(high_post, late_high_post, post_arrival_ticks / 17)
        << "equal ticket counts receive the same future share";
    EXPECT_LT(GetTask(late_high)->runtime, GetTask(high)->runtime)
        << "lottery does not catch a late task up to an incumbent with the same priority";
    VisualizeTasks();
}

TEST_F(SchedulerTest, schedule__fair_share_balances_users)
{
    InitScheduler(SCHEDULING_ALGORITHM_FAIR_SHARE_WITH_PRIORITY);
    auto user_a = create_task(8, 0, 1);
    auto user_b1 = create_task(8, 0, 2);
    auto user_b2 = create_task(8, 0, 2);
    auto user_b3 = create_task(8, 0, 2);
    ASSERT_GT(user_a, 0);
    ASSERT_GT(user_b1, 0);
    ASSERT_GT(user_b2, 0);
    ASSERT_GT(user_b3, 0);
    //ASSERT_EQ(block_task(0), 0);

    const int ticks = 2401;
    int uid0_hits = 0;
    int uid1_hits = 0;
    int uid2_hits = 0;
    for (int i = 0; i < ticks; i++)
    {
        tPid pid = schedule();
        const tTaskStruct *task = GetTask(pid);
        ASSERT_NE(task, nullptr);
        if (task->uid == 0)
        {
            uid0_hits++;
        }
        else if (task->uid == 1)
        {
            uid1_hits++;
        }
        else if (task->uid == 2)
        {
            uid2_hits++;
        }
    }

    EXPECT_EQ(uid0_hits + uid1_hits + uid2_hits, ticks);
    EXPECT_NEAR(uid0_hits, ticks / 3, ticks / 100) << "user 0 should get roughly third of CPU";
    EXPECT_NEAR(uid1_hits, ticks / 3, ticks / 100) << "user 1 should get roughly third of CPU";
    EXPECT_NEAR(uid2_hits, ticks / 3, ticks / 100) << "user 2 should get roughly third of CPU";

    const int per_task = uid2_hits / 3;
    ExpectTaskRuntimeNear(0, uid0_hits, ticks / 100, "user 1 task should get roughly third of CPU");
    ExpectTaskRuntimeNear(user_a, uid1_hits, ticks / 100, "user 1 task should get roughly third of CPU");
    ExpectTaskRuntimeNear(user_b1, per_task, ticks / 100, "equal-priority tasks of user 2 should share CPU evenly");
    ExpectTaskRuntimeNear(user_b2, per_task, ticks / 100, "equal-priority tasks of user 2 should share CPU evenly");
    ExpectTaskRuntimeNear(user_b3, per_task, ticks / 100, "equal-priority tasks of user 2 should share CPU evenly");
    EXPECT_GT(GetTask(user_b1)->runtime, 0) << "no task of user 2 should be starved";
    EXPECT_GT(GetTask(user_b2)->runtime, 0) << "no task of user 2 should be starved";
    EXPECT_GT(GetTask(user_b3)->runtime, 0) << "no task of user 2 should be starved";
    VisualizeTasks();
}

TEST_F(SchedulerTest, schedule__fair_share_balances_users_with_priority)
{
    InitScheduler(SCHEDULING_ALGORITHM_FAIR_SHARE_WITH_PRIORITY);
    tPid user_a = 0;
    auto user_b1 = create_task(4, 0, 2);
    auto user_b2 = create_task(8, 0, 2);
    auto user_b3 = create_task(12, 0, 2);
    ASSERT_GT(user_b1, 0);
    ASSERT_GT(user_b2, 0);
    ASSERT_GT(user_b3, 0);

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
        if (task->uid == 0)
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

    ExpectTaskRuntimeNear(user_a, uid1_hits, ticks / 100, "user 1 task should get roughly half of CPU");
    ExpectTaskRuntimeNear(user_b1, uid2_hits * tickets_b1 / tickets_total, ticks / 100,
                          "priority 4 task should get 5/27 of user 2 share");
    ExpectTaskRuntimeNear(user_b2, uid2_hits * tickets_b2 / tickets_total, ticks / 100,
                          "priority 8 task should get 9/27 of user 2 share");
    ExpectTaskRuntimeNear(user_b3, uid2_hits * tickets_b3 / tickets_total, ticks / 100,
                          "priority 12 task should get 13/27 of user 2 share");
    EXPECT_GT(b3_hits, b2_hits) << "higher priority task should run more within user 2";
    EXPECT_GT(b2_hits, b1_hits) << "higher priority task should run more within user 2";
    EXPECT_GT(GetTask(user_b1)->runtime, 0) << "no task of user 2 should be starved";
    VisualizeTasks();
}

TEST_F(SchedulerTest, schedule__fair_share_uses_priority_within_user)
{
    InitScheduler(SCHEDULING_ALGORITHM_FAIR_SHARE_WITH_PRIORITY);
    auto low = 0;
    auto high = CreateTask(15, 0, 0);

    const int ticks = 1700;
    RunScheduler(ticks);

    auto low_hits = GetTask(low)->runtime;
    auto high_hits = GetTask(high)->runtime;

    EXPECT_EQ(low_hits + high_hits, ticks);

    const int low_tickets = TicketCount(0);
    const int high_tickets = TicketCount(15);
    const int tickets_total = low_tickets + high_tickets;

    ExpectTaskRuntimeNear(low, ticks * low_tickets / tickets_total, ticks / 100,
                          "low-priority task should get 1/17 of CPU");
    ExpectTaskRuntimeNear(high, ticks * high_tickets / tickets_total, ticks / 100,
                          "high-priority task should get 15/17 of CPU");
    EXPECT_GT(high_hits, low_hits * 2)
        << "within one user, higher priority should run noticeably longer";
    VisualizeTasks();
}

TEST_F(SchedulerTest, schedule__fair_share_balances_late_arrival)
{
    InitScheduler(SCHEDULING_ALGORITHM_FAIR_SHARE_WITH_PRIORITY);
    auto user_b1 = create_task(8, 0, 2);
    ASSERT_GT(user_b1, 0);

    const int warmup_ticks = 2400;
    const int post_arrival_ticks = 2400;
    RunScheduler(warmup_ticks);
    VisualizeTasks();

    const int b1_before = GetTask(user_b1)->runtime;
    ASSERT_GT(b1_before, 0);

    auto user_b2 = create_task(8, 0, 2);
    ASSERT_GT(user_b2, 0);
    EXPECT_EQ(GetTask(user_b2)->start_time, scheduler_->total_runtime);

    int uid1_post = 0;
    int uid2_post = 0;
    int b1_post = 0;
    int b2_post = 0;
    for (int i = 0; i < post_arrival_ticks; i++)
    {
        tPid pid = schedule();
        const tTaskStruct *task = GetTask(pid);
        ASSERT_NE(task, nullptr);
        if (task->uid == 0)
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
    EXPECT_NEAR(b1_post, uid2_post / 2, post_arrival_ticks / 100)
        << "after arrival, equal-priority tasks should split only future CPU";
    EXPECT_NEAR(b2_post, uid2_post / 2, post_arrival_ticks / 100)
        << "late arrival should receive its future share without catching up the past";
    EXPECT_GT(GetTask(user_b1)->runtime, GetTask(user_b2)->runtime + b1_before / 2)
        << "time before start_time must not be repaid to the new task";
    EXPECT_GT(b1_post, 0) << "incumbent must keep receiving CPU after the new task arrives";
    VisualizeTasks();
}

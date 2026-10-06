#include <stdint.h>
#include <stddef.h>

#include "scheduler.h"
#include "task.h"
#include "types.h"

static tScheduler *g_scheduler = NULL;
static uint32_t g_rng = 1;

static uint32_t next_rand(void)
{
    g_rng = g_rng * 1103515245u + 12345u;
    return g_rng;
}

static int is_ready(const tTaskStruct *task)
{
    return task != NULL && task->pid >= 0 && task->state == TASK_STATE_READY;
}

static tTaskStruct *find_task_in_mgr(tPid pid)
{
    const tTaskMgr *mgr = get_taskMgr();
    if (mgr == NULL || pid < 0)
    {
        return NULL;
    }

    for (uint8_t i = 0; i < mgr->size; i++)
    {
        if (mgr->tasks[i].pid == pid)
        {
            return &mgr->tasks[i];
        }
    }

    return NULL;
}

static int task_index(tPid pid)
{
    const tTaskMgr *mgr = get_taskMgr();
    if (mgr == NULL)
    {
        return -1;
    }

    for (uint8_t i = 0; i < mgr->size; i++)
    {
        if (mgr->tasks[i].pid == pid)
        {
            return i;
        }
    }

    return -1;
}

static int32_t priority_quantum(const tTaskStruct *task)
{
    if (task == NULL)
    {
        return 1;
    }
    return task->priority + 1;
}

static int32_t ticket_count(const tTaskStruct *task)
{
    if (task == NULL)
    {
        return 0;
    }
    return (int32_t)task->priority + 1;
}

static tPid next_ready_after(tPid pid)
{
    const tTaskMgr *mgr = get_taskMgr();
    if (mgr == NULL || mgr->size == 0)
    {
        return -1;
    }

    int start = task_index(pid);
    for (uint8_t k = 1; k <= mgr->size; k++)
    {
        int i = (start + k) % mgr->size;
        if (is_ready(&mgr->tasks[i]))
        {
            return mgr->tasks[i].pid;
        }
    }

    return -1;
}

static tPid keep_or_rr(int32_t quantum)
{
    tTaskStruct *current = find_task_in_mgr(g_scheduler->current_task);
    if (is_ready(current) && g_scheduler->task_runtime < quantum)
    {
        return current->pid;
    }

    tPid next = next_ready_after(g_scheduler->current_task);
    if (next < 0 && is_ready(current))
    {
        return current->pid;
    }
    return next;
}

static tPid pick_round_robin(void)
{
    return keep_or_rr(1);
}

static tPid pick_round_robin_priority(void)
{
    tTaskStruct *current = find_task_in_mgr(g_scheduler->current_task);
    return keep_or_rr(priority_quantum(current));
}

static tPid pick_random_priority(void)
{
    const tTaskMgr *mgr = get_taskMgr();
    int32_t total = 0;

    for (uint8_t i = 0; i < mgr->size; i++)
    {
        if (is_ready(&mgr->tasks[i]))
        {
            total += ticket_count(&mgr->tasks[i]);
        }
    }

    if (total <= 0)
    {
        return -1;
    }

    int32_t ticket = (int32_t)(next_rand() % (uint32_t)total);
    for (uint8_t i = 0; i < mgr->size; i++)
    {
        if (!is_ready(&mgr->tasks[i]))
        {
            continue;
        }
        ticket -= ticket_count(&mgr->tasks[i]);
        if (ticket < 0)
        {
            return mgr->tasks[i].pid;
        }
    }

    return -1;
}

static int count_ready_users(void)
{
    const tTaskMgr *mgr = get_taskMgr();
    uint8_t seen[256];
    int count = 0;

    for (int i = 0; i < 256; i++)
    {
        seen[i] = 0;
    }

    for (uint8_t i = 0; i < mgr->size; i++)
    {
        if (is_ready(&mgr->tasks[i]) && !seen[mgr->tasks[i].uid])
        {
            seen[mgr->tasks[i].uid] = 1;
            count++;
        }
    }

    return count;
}

static int32_t uid_runtime(uint8_t uid)
{
    const tTaskMgr *mgr = get_taskMgr();
    int32_t sum = 0;

    for (uint8_t i = 0; i < mgr->size; i++)
    {
        if (is_ready(&mgr->tasks[i]) && mgr->tasks[i].uid == uid)
        {
            sum += mgr->tasks[i].runtime;
        }
    }

    tTaskStruct *current = find_task_in_mgr(g_scheduler->current_task);
    if (is_ready(current) && current->uid == uid)
    {
        sum += g_scheduler->task_runtime;
    }

    return sum;
}

static uint8_t pick_uid_fair_share(void)
{
    const tTaskMgr *mgr = get_taskMgr();
    int users = count_ready_users();
    int32_t fair_target = g_scheduler->total_runtime / users;

    uint8_t best_uid = 0;
    int32_t best_deficit = INT32_MIN;
    int found = 0;
    uint8_t seen[256];

    for (int i = 0; i < 256; i++)
    {
        seen[i] = 0;
    }

    for (uint8_t i = 0; i < mgr->size; i++)
    {
        if (!is_ready(&mgr->tasks[i]))
        {
            continue;
        }

        uint8_t uid = mgr->tasks[i].uid;
        if (seen[uid])
        {
            continue;
        }
        seen[uid] = 1;

        int32_t deficit = fair_target - uid_runtime(uid);
        if (!found || deficit > best_deficit || (deficit == best_deficit && uid < best_uid))
        {
            best_deficit = deficit;
            best_uid = uid;
            found = 1;
        }
    }

    return best_uid;
}

static int32_t effective_runtime(const tTaskStruct *task)
{
    int32_t runtime = task->runtime;
    if (task->pid == g_scheduler->current_task)
    {
        runtime += g_scheduler->task_runtime;
    }
    return runtime;
}

static int32_t expected_runtime(const tTaskStruct *task)
{
    const tTaskMgr *mgr = get_taskMgr();
    int32_t now = g_scheduler->total_runtime;
    int32_t marks[18];
    int nmarks = 0;

    if (task == NULL || now <= task->start_time)
    {
        return 0;
    }

    marks[nmarks++] = task->start_time;
    marks[nmarks++] = now;

    for (uint8_t i = 0; i < mgr->size; i++)
    {
        int32_t start;

        if (!is_ready(&mgr->tasks[i]))
        {
            continue;
        }

        start = mgr->tasks[i].start_time;
        if (start > task->start_time && start < now && nmarks < 18)
        {
            marks[nmarks++] = start;
        }
    }

    for (int i = 1; i < nmarks; i++)
    {
        int32_t value = marks[i];
        int j = i;
        while (j > 0 && marks[j - 1] > value)
        {
            marks[j] = marks[j - 1];
            j--;
        }
        marks[j] = value;
    }

    int unique = 0;
    for (int i = 0; i < nmarks; i++)
    {
        if (unique == 0 || marks[i] != marks[unique - 1])
        {
            marks[unique++] = marks[i];
        }
    }

    int32_t expected = 0;
    for (int i = 0; i + 1 < unique; i++)
    {
        int32_t t0 = marks[i];
        int32_t len = marks[i + 1] - t0;
        int users = 0;
        int32_t tickets = 0;
        uint8_t seen[256];

        if (len <= 0)
        {
            continue;
        }

        for (int s = 0; s < 256; s++)
        {
            seen[s] = 0;
        }

        for (uint8_t k = 0; k < mgr->size; k++)
        {
            if (!is_ready(&mgr->tasks[k]) || mgr->tasks[k].start_time > t0)
            {
                continue;
            }

            if (!seen[mgr->tasks[k].uid])
            {
                seen[mgr->tasks[k].uid] = 1;
                users++;
            }
            if (mgr->tasks[k].uid == task->uid)
            {
                tickets += ticket_count(&mgr->tasks[k]);
            }
        }

        if (users <= 0 || tickets <= 0)
        {
            continue;
        }

        expected += (len / users) * ticket_count(task) / tickets;
    }

    return expected;
}

static int rr_distance(int index, int current_index, int size)
{
    int distance;

    if (index < 0 || size <= 0)
    {
        return size + 1;
    }

    distance = (index - current_index + size) % size;
    if (distance == 0)
    {
        distance = size;
    }
    return distance;
}

static tPid pick_task_of_uid_fair_share(uint8_t uid)
{
    const tTaskMgr *mgr = get_taskMgr();
    tPid best = -1;
    int best_index = -1;
    int32_t best_deficit = INT32_MIN;
    int current_index = task_index(g_scheduler->current_task);

    for (uint8_t i = 0; i < mgr->size; i++)
    {
        int32_t deficit;
        int better;

        if (!is_ready(&mgr->tasks[i]) || mgr->tasks[i].uid != uid)
        {
            continue;
        }

        deficit = expected_runtime(&mgr->tasks[i]) - effective_runtime(&mgr->tasks[i]);
        better = best < 0 || deficit > best_deficit;
        if (!better && deficit == best_deficit)
        {
            better = rr_distance(i, current_index, mgr->size) <
                     rr_distance(best_index, current_index, mgr->size);
        }

        if (better)
        {
            best_deficit = deficit;
            best = mgr->tasks[i].pid;
            best_index = i;
        }
    }

    return best;
}

static tPid pick_fair_share(void)
{
    tTaskStruct *current = find_task_in_mgr(g_scheduler->current_task);
    int users = count_ready_users();

    if (users <= 0)
    {
        return -1;
    }

    if (users == 1)
    {
        return pick_round_robin_priority();
    }

    if (is_ready(current) && g_scheduler->task_runtime < priority_quantum(current))
    {
        return current->pid;
    }

    uint8_t uid = pick_uid_fair_share();
    return pick_task_of_uid_fair_share(uid);
}

static tPid pick_next(void)
{
    switch (g_scheduler->scheduling_algorithm)
    {
        case SCHEDULING_ALGORITHM_ROUND_ROBIN_WITH_PRIORITY:
            return pick_round_robin_priority();
        case SCHEDULING_ALGORITHM_RANDOM_WITH_PRIORITY:
            return pick_random_priority();
        case SCHEDULING_ALGORITHM_FAIR_SHARE_WITH_PRIORITY:
            return pick_fair_share();
        case SCHEDULING_ALGORITHM_ROUND_ROBIN:
        default:
            return pick_round_robin();
    }
}

static void reap_zombies(const tTaskStruct *parent)
{
    const tTaskMgr *mgr = get_taskMgr();
    if (mgr == NULL || parent == NULL)
    {
        return;
    }

    for (uint8_t i = 0; i < mgr->size; i++)
    {
        if (mgr->tasks[i].pid > 0 &&
            mgr->tasks[i].state == TASK_STATE_TERMINATED &&
            mgr->tasks[i].ppid == parent->pid)
        {
            destroy_task(mgr->tasks[i].pid);
        }
    }
}

static void handle_signals(tTaskStruct *task)
{
    if (task == NULL || task->pid < 0)
    {
        return;
    }

    if (task->signals & TASK_SIG_CHILD)
    {
        reap_zombies(task);
        task->signals &= (uint8_t)~TASK_SIG_CHILD;
    }

    if (task->signals & TASK_SIG_TERM)
    {
        task->signals &= (uint8_t)~TASK_SIG_TERM;
        if (task->pid != 0)
        {
            terminate_task(task->pid, 0);
        }
    }
}

static tPid pick_runnable(void)
{
    const tTaskMgr *mgr = get_taskMgr();
    tPid next = pick_next();

    for (uint8_t i = 0; i < mgr->size && next >= 0; i++)
    {
        tTaskStruct *task = find_task_in_mgr(next);
        handle_signals(task);
        if (is_ready(task))
        {
            return next;
        }
        next = next_ready_after(next);
    }

    return -1;
}

void init_scheduler(tScheduler *scheduler)
{
    g_scheduler = scheduler;
    g_rng = 1;
    if (scheduler == NULL)
    {
        return;
    }

    scheduler->current_task = -1;
    scheduler->task_runtime = 0;
    scheduler->total_runtime = -1;
}

void destroy_scheduler(void)
{
    g_scheduler = NULL;
}

int32_t get_total_runtime(void)
{
    if (g_scheduler == NULL)
    {
        return 0;
    }
    return g_scheduler->total_runtime;
}

tPid schedule(void)
{
    if (g_scheduler == NULL)
    {
        return -1;
    }

    const tTaskMgr *mgr = get_taskMgr();
    if (mgr == NULL)
    {
        return -1;
    }

    // increase the runtime of previous task
    if ( g_scheduler->current_task >= 0)
        g_scheduler->task_runtime++;

    g_scheduler->total_runtime++;

    tPid next = pick_runnable();
    if (next != g_scheduler->current_task)
    {
        tTaskStruct *old = find_task_in_mgr(g_scheduler->current_task);
        if (old != NULL)
        {
            old->runtime += g_scheduler->task_runtime;
        }
        g_scheduler->current_task = next;
        g_scheduler->task_runtime = 0;
    }

    return g_scheduler->current_task;
}

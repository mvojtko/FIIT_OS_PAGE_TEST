#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "task.h"
#include "types.h"
#include "scheduler.h"

static tTaskMgr *g_task_mgr = NULL;

static void set_free_slot(tTaskStruct *task)
{
    memset(task, 0, sizeof(*task));
    task->pid = -1;
    task->ppid = -1;
}

static tTaskStruct *find_task(tPid pid)
{
    if (g_task_mgr == NULL || pid < 0)
    {
        return NULL;
    }

    for (uint8_t i = 0; i < g_task_mgr->size; i++)
    {
        if (g_task_mgr->tasks[i].pid == pid)
        {
            return &g_task_mgr->tasks[i];
        }
    }

    return NULL;
}

static tTaskStruct *find_free_slot(void)
{
    for (uint8_t i = 0; i < g_task_mgr->size; i++)
    {
        if (g_task_mgr->tasks[i].pid == -1)
        {
            return &g_task_mgr->tasks[i];
        }
    }

    return NULL;
}

static int is_zombie(const tTaskStruct *task)
{
    return task != NULL && task->state == TASK_STATE_TERMINATED;
}

static tPid allocate_pid(void)
{
    for (tPid pid = 1; pid < INT16_MAX; pid++)
    {
        if (find_task(pid) == NULL)
        {
            return pid;
        }
    }

    return -1;
}

int init_taskMgr(tTaskMgr *task_mgr)
{
    if (task_mgr == NULL || task_mgr->tasks == NULL || task_mgr->size == 0)
    {
        return -1;
    }

    for (uint8_t i = 0; i < task_mgr->size; i++)
    {
        set_free_slot(&task_mgr->tasks[i]);
    }

    task_mgr->tasks[0].pid = 0;
    task_mgr->tasks[0].ppid = -1;

    g_task_mgr = task_mgr;

    return 0;
}

void destroy_taskMgr(void)
{
    g_task_mgr = NULL;
}

const tTaskMgr *get_taskMgr(void)
{
    return g_task_mgr;
}

tPid create_task(uint8_t priority, tPid ppid, uint8_t uid)
{
    if (g_task_mgr == NULL)
    {
        return -3;
    }

    if (ppid < 0 || priority > 15)
    {
        return -2;
    }

    tTaskStruct *parent = find_task(ppid);
    if (parent == NULL || is_zombie(parent))
    {
        return -4;
    }

    tTaskStruct *slot = find_free_slot();
    if (slot == NULL)
    {
        return -1;
    }

    tPid pid = allocate_pid();
    if (pid < 0)
    {
        return -1;
    }

    memset(slot, 0, sizeof(*slot));
    slot->pid = pid;
    slot->ppid = ppid;
    slot->priority = priority;
    slot->state = TASK_STATE_READY;
    slot->uid = uid;
    slot->start_time = get_total_runtime();

    return slot->pid;
}

int terminate_task(tPid pid, int exit_code)
{
    if (g_task_mgr == NULL)
    {
        return -3;
    }

    if (pid == 0)
    {
        return -1;
    }

    tTaskStruct *task = find_task(pid);
    if (task == NULL || is_zombie(task))
    {
        return -1;
    }

    task->state = TASK_STATE_TERMINATED;
    task->exit_code = exit_code;

    tTaskStruct *parent = find_task(task->ppid);
    if (parent != NULL)
    {
        parent->signals |= TASK_SIG_CHILD;
    }

    return 0;
}

int destroy_task(tPid pid)
{
    if (g_task_mgr == NULL)
    {
        return -3;
    }

    if (pid == 0)
    {
        return -1;
    }

    tTaskStruct *task = find_task(pid);
    if (task == NULL)
    {
        return -1;
    }

    set_free_slot(task);
    return 0;
}

int send_signal(tPid pid, uint8_t signal)
{
    if (g_task_mgr == NULL)
    {
        return -3;
    }

    tTaskStruct *task = find_task(pid);
    if (task == NULL || is_zombie(task))
    {
        return -1;
    }

    task->signals |= signal;
    return 0;
}

int block_task(tPid pid)
{
    if (g_task_mgr == NULL)
    {
        return -3;
    }

    tTaskStruct *task = find_task(pid);
    if (task == NULL || is_zombie(task))
    {
        return -1;
    }

    task->state = TASK_STATE_BLOCKED;
    return 0;
}

int unblock_task(tPid pid)
{
    if (g_task_mgr == NULL)
    {
        return -3;
    }

    tTaskStruct *task = find_task(pid);
    if (task == NULL || is_zombie(task))
    {
        return -1;
    }

    task->state = TASK_STATE_READY;
    return 0;
}

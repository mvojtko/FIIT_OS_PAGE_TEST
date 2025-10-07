#include <string.h>

#include "task.h"
#include "ram.h"

static tTaskMgr *g_task_mgr = NULL;

#define MAX_NUM_TASKS sizeof(g_task_mgr->tasks)/sizeof(tTaskStruct)
#define NUM_FRAMES(bytes) ((bytes) / ram->page_size) + (((bytes) % ram->page_size) ? 1 : 0)

int init_taskMgr()
{
    const tRam *ram = get_ram_state();
    if (ram == NULL)
        return -1;

    g_task_mgr = (tTaskMgr *)falloc(NUM_FRAMES(sizeof(tTaskMgr)));
    
    if (g_task_mgr == NULL)
        return -1;

    for (uint8_t id = 0; id < 8; id++)
    {
        g_task_mgr->tasks[id].pid = -1;
    }

    return 0;
}

void destroy_taskMgr()
{
    const tRam *ram = get_ram_state();
    if (ram == NULL)
        return;

    ffree((void *)g_task_mgr, NUM_FRAMES(sizeof(tTaskMgr)));
    g_task_mgr = NULL;
}

int create_task(const tPageTableEntry *page_table, uint8_t max_frames)
{
    if (page_table == NULL)
        return -2;

    if (g_task_mgr == NULL)
        return -3;

    for (uint8_t id = 0; id < MAX_NUM_TASKS; id ++)
    {
        tTaskStruct *task = &(g_task_mgr->tasks[id]);
        if (task->pid == -1)
        {
            task->max_frames = max_frames;
            task->pid = id;
            memcpy(&task->page_table, page_table, 8*sizeof(tPageTableEntry));
            return id;
        }
    }
    return -1;  
}

int destroy_task(int pid)
{
    for (uint8_t id = 0; id < MAX_NUM_TASKS; id ++)
    {
        tTaskStruct *task = &(g_task_mgr->tasks[id]);
        if (pid == task->pid)
        {
            memset(task, 0, sizeof(tTaskStruct));
            task->pid = -1;
            return 0;
        }
    }
    return -1;
}

const tTaskMgr *get_task_mgr()
{
    return g_task_mgr;
}

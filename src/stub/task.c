#include <stdint.h>

#include "task.h"
#include "types.h"

int init_taskMgr(tTaskMgr *task_mgr)
{
    (void)task_mgr;
    return 0;
}

void destroy_taskMgr()
{

}

tPid create_task(int priority, tPid ppid, int uid)
{
    (void)priority;
    (void)ppid;
    (void)uid;
    return 0;
}

int terminate_task(tPid pid, int exit_code)
{
    (void)pid;
    (void)exit_code;
    return 0;
}

int destroy_task(tPid pid)
{
    (void)pid;
    return 0;
}

int send_signal(tPid pid, int signal)
{
    (void)pid;
    (void)signal;
    return 0;
}

int block_task(tPid pid)
{
    (void)pid;
    return 0;
}

int unblock_task(tPid pid)
{
    (void)pid;
    return 0;
}

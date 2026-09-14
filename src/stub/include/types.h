#pragma once

#include <stdint.h>

#define TASK_STATE_READY 0x0
#define TASK_STATE_BLOCKED 0x1
#define TASK_STATE_TERMINATED 0x3

#define TASK_SIG_TERM 0x1
#define TASK_SIG_CHILD 0x2

typedef int tPid;

typedef struct tTaskStruct
{
    tPid pid;              // Process ID of the task.
    tPid ppid;             // Parent process ID of the task.
    int priority;         // Priority of the task.
    int signals;          // Signals of the task.
    int state;            // State of the task.
    int exit_code;        // Exit code of the task.
    int runtime;          // total runtime of the task.
    int uid;              // User ID of the task.
} tTaskStruct;
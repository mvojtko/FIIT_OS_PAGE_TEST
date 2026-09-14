#pragma once

#include <stdint.h>

#include "types.h"

typedef struct tTaskMgr
{
    tTaskStruct *tasks; // Storage for data of tasks.
    int size; // Maximum number of tasks in the task manager.
} tTaskMgr;

// Initializes the task manager.
// This function:
//   - Takes a pointer to the tTaskMgr structure and initialzies all elements.
// Returns:
//    0  Success.
//   -1  Invalid input parameters.
int init_taskMgr(tTaskMgr *task_mgr);

// Destroys the task manager and releases all resources.
void destroy_taskMgr();

// Creates a new task in memory.
// This function:
//   - Finds and fills a tTaskStruct entry in the task manager.
//   - Assigns a PID to the task.
//   - Sets the parent process ID to the given ppid.
//   - Sets the user ID to the given uid.
//   - Sets the priority to the given priority.
//   - Sets all other fields to default values.
// Returns:
//    PID on success.
//   -1  Not enough resources to create a new task.
//   -2  Invalid input parameters.
//   -3  The system was not initialized.
tPid create_task(int priority, tPid ppid, int uid);

// Terminates a task.
// This function:
//   - Cleans and marks the corresponding tTaskStruct entry in the task manager as terminated.
//   - Stores exit code to its tTaskStruct entry.
//   - Signals parent process that the child has been destroyed.
// Returns:
//    0  Success.
//   -1  Task does not exist.
int terminate_task(tPid pid, int exit_code);

// Destroys a task.
// This function:
//   - Cleans corresponding tTaskStruct entry in the task manager nad sets PID to -1.
// Returns:
//    0  Success.
//   -1  Task does not exist.
int destroy_task(tPid pid);

// Sends a signal to a task.
// This function:
//   - Sends a signal to the corresponding tTaskStruct entry in the task manager.
// Returns:
//    0  Success.
//   -1  Task does not exist.
int send_signal(tPid pid, int signal);

// Blocks a task.
// This function:
//   - Blocks the corresponding tTaskStruct entry in the task manager.
//     This causes the task will not be scheduled until it is unblocked.
// Returns:
//    0  Success.
//   -1  Task does not exist.
int block_task(tPid pid);

// Unblocks a task.
// This function:
//   - Unblocks the corresponding tTaskStruct entry in the task manager.
// Returns:
//    0  Success.
//   -1  Task does not exist.
int unblock_task(tPid pid);

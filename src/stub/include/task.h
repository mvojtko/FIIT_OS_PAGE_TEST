#pragma once

#include <stdint.h>

#include "types.h"

#define TASK_TABLE_SIZE 8

typedef struct tTaskStruct
{
    uint8_t max_frames;   // Limits the maximum number of task pages in RAM. If 0, there is no limit.
    int pid;              // Process ID of the task.
    void *address_space;  // Handle to the content of the task's virtual address space.
    tPageTableEntry page_table[PAGE_TABLE_SIZE];  // The task`s page table.
} tTaskStruct;

typedef struct tTaskMgr
{
    tTaskStruct tasks[TASK_TABLE_SIZE];  // Storage for task data.
} tTaskMgr;

// Initializes the task manager.
// This function:
//   - Reserves space in RAM for tTaskMgr (i.e. consumes one or more frames).
// Returns:
//    0  - Success.
//   -1  - Not enough resources.
int init_taskMgr();

// Destroys the task manager and releases all resources in RAM.
void destroy_taskMgr();

// Creates a new task in memory.
// This function:
//   - Finds and fills a tTaskStruct entry in the task manager.
//   - Assigns a PID to the task.
//   - Copies the initial page table of the task.
//   - Expects that no initial frames are consumed by this function.
// Returns:
//    PID on success.
//   -1  Not enough resources to create a new task.
//   -2  Invalid input parameters.
//   -3  The system was not initialized.
int create_task(const tPageTableEntry *page_table, uint8_t max_frames, void *address_space);

// Destroys a task in memory.
// This function:
//   - Cleans and marks the corresponding tTaskStruct entry in the task manager as free (sets PID to -1).
//   - Releases all frames of the task from RAM.
// Returns:
//    0  Success.
//   -1  Task does not exist.
int destroy_task(int pid);

// Returns a pointer to the tTaskMgr structure in RAM.
// Returns:
//   Pointer to tTaskMgr.
//   nullptr if the task manager is not initialized.
const tTaskMgr *get_task_mgr();

// Returns a pointer to the tTaskStruct for the given PID.
// Returns:
//   Pointer to the existing task.
//   nullptr if the task was not found.
tTaskStruct *get_task_struct(int pid);

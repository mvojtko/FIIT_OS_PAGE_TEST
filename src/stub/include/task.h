#pragma once

#include <stdint.h>

typedef struct tTaskStruct
{
    uint8_t max_frames;  // limit maximum number of pages in ram. If 0 there is no limit
    int pid;             // process id of task
    void *page_table;    // handle to page_table in ram.
    void *swap_table;    // handle to swap_table in ram. If null swap is not used by task
} tTaskStruct;

typedef struct tTaskMgr
{
    tTaskStruct[10] tasks;  // storage for task data
} tTaskMgr;

// initializes taskMgr.
// this function:
//   reserves space in ram for tTaskMgr - i.e. this should consume some frame/frames
int init_taskMgr();

// Destroy taskMgr. Releases all resources.
void destroy_taskMgr();

// creates new task in the memory.
// this function:
//   fills task_struct entry in the memory
//   assigns pid to the task
//   stores page-table for the task
//   optionally creates swap table if swap was initialized
// The address space of the task has size 2^16 B
// returns:  pid on success
//          -1 not enough resources to create new task
//          -2 when input parameters are invalid
//          -3 the system was not initialized
int create_task(void *page_table, uint8_t max_frames);

// destroy task in the memory
// this function:
//   frees task_struct entry in the memory
//   releases all pages of the task
//   destroys page-table of the task
// returns:  0 success
//          -1 task does not exist
int destroy_task(int pid);

// Returns pointer to a tTaskMgr structure.
//  returns - pointer to tTaskMgr
//          - nullptr when task_mgr is not initialized
tTaskMgr *get_task_mgr();

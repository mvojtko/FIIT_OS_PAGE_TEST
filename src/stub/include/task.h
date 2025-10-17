#pragma once

#include <stdint.h>

#include "types.h"

#define TASK_TABLE_SIZE 8

typedef struct tTaskStruct
{
    uint8_t max_frames;    // limit maximum number of pages in ram. If 0 there is no limit
    int pid;               // process id of task
    void * address_space;  // handle to content of virtual address space of task 
    tPageTableEntry page_table[PAGE_TABLE_SIZE];  // page_table has always size of 8 pages
} tTaskStruct;

typedef struct tTaskMgr
{
    tTaskStruct tasks[TASK_TABLE_SIZE];  // storage for task data
} tTaskMgr;

// initializes taskMgr.
// this function:
//   reserves space in ram for tTaskMgr - i.e. this should consume some frame/frames
//  result  0 - success
//         -1 - not enough resources
int init_taskMgr();

// Destroy taskMgr. Releases all resources.
void destroy_taskMgr();

// creates new task in the memory.
// this function:
//   - fills task_struct entry in the task mgr
//   - assigns pid to the task
//   - copies initial page-table of the task
//   - expect that no initial frames are consumed in this function
// returns:  pid on success
//          -1 not enough resources to create new task
//          -2 when input parameters are invalid
//          -3 the system was not initialized
int create_task(const tPageTableEntry *page_table, uint8_t max_frames, void *address_space);

// destroy task in the memory
// this function:
//   marks free task_struct entry in the task mgr i.e. sets pid to -1
//   releases all pages of the task
//   destroys page-table of the task
// returns:  0 success
//          -1 task does not exist
int destroy_task(int pid);

// Returns pointer to a tTaskMgr structure in the ram.
//  returns - pointer to tTaskMgr
//          - nullptr when task_mgr is not initialized
const tTaskMgr *get_task_mgr();

// TODO test this
tTaskStruct *get_task_struct(int pid);

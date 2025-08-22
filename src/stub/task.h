#pragma once

#include <stdint.h>

// This is task segment containing task data within address-space
typedef struct tSegment
{
  uint8_t r : 1;
  uint8_t w : 1;
  uint8_t x : 1;
  uint16_t first_page_id;
  uint16_t page_count;
} tSegment;

typedef struct tTask_struct
{
    int pid;
    void *page_table;
    tSegment segments[];
    uint8_t segment_count;
} tTask_struct;

// creates new task in the memory.
// this function:
//   fills task_struct entry in the memory
//   assigns pid to the task
//   creates page-table for the task
//   segments - initial task segments
// The address space of the task has size 2^16 B
// returns:  pid on success
//          -1 not enough resources to create new task
//          -2 when input parameters are invalid
int create_task(tSegment segments[], uint8_t segment_count);

// destroy task in the memory
// this function:
//   frees task_struct entry in the memory
//   releases all pages of the task
//   destroys page-table of the task
// returns:  0 success
//          -1 task does not exist
int destroy_task(int pid);

// produces tasks stats data to provided buffer
// returns  0 success
//          1 buffer contains truncated data 
// Format of the table output in the buffer:
// task page-table 
// %d   %p
// ...  ...
// tasks total: %u
int dump_tasks_stats(const char *buffer, uint16_t size);

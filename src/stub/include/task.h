#pragma once

#include <stdint.h>

// Task data within address-space
typedef struct tTask_data
{
  uint8_t r : 1;          // read access
  uint8_t w : 1;          // write access
  uint8_t x : 1;          // execute access
  uint16_t first_page_id; // placement of data in task address_space
  uint16_t page_count;    // number of pages
} tTask_data;

typedef struct tTask_struct
{
  int pid;           // process id of task
  void *page_table;  // handle to page_table in ram
  void *swap_table;  // handle to swap_table in ram if null swap is not used by task
} tTask_struct;

// creates new task in the memory.
// this function:
//   fills task_struct entry in the memory
//   assigns pid to the task
//   creates page-table for the task
//   optionaly creates swap table if swap was initialized
// The address space of the task has size 2^16 B
// returns:  pid on success
//          -1 not enough resources to create new task
//          -2 when input parameters are invalid
int create_task(tTask_data data_blocks[], uint8_t block_count);

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

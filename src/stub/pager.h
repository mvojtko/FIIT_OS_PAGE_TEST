#pragma once

#include <stdint.h>

typedef struct task_struct
{
    int pid;
    void *page_table;
} task_struct;

// Init ram model used for testing. All data in memory are zeroed.
// Library can use only this memory for paging maintanance
//   memory    - pointer to the memory
//   size      - size of the memory
//   page_size - size of one page in the memory 
//   returns - n number of pages in the memory
//           - -1 size is not power of two
//           - -2 page_size not power of two 
int init_ram(void *memory, uint16_t size, uint8_t page_size);

// produces ram stats data to provided buffer
// returns 0 on success
//         -1 buffer contains truncated data 
// Format of the output in the buffer:
// free pages:            %u
// used pages by system:  %u
// used pages by tasks:   %u
// pages total:           %u
int dump_ram_stats(const char *buffer, uint16_t size);

// check whether library supports thread safety
// if yes paging may be tested by multiple threads
int is_thread_safe();

// creates new task in the memory.
// this function:
//   fills task_struct entry in the memory
//   assigns pid to the task
//   creates page-table for the task
// The address space of the task has size 2^16 B
// returns: pid on success
//          -1 when there is not enough resources to create new task 
int create_task();

// destroy task in the memory
// this function:
//   frees task_struct entry in the memory
//   releases all pages of the task
//   destroys page-table of the task
// returns: 0 on success
//          -1 when the task does not exist
int destroy_task(int pid);

// produces tasks stats data to provided buffer
// returns 0 on success
//         -1 buffer contains truncated data 
// Format of the table output in the buffer:
// task page-table 
// %d   %p
// ...  ...
// tasks total: %u
int dump_tasks_stats(const char *buffer, uint16_t size);

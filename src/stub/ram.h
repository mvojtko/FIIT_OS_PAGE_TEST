#pragma once

#include <stdint.h>

// Init ram model. Library can use only this memory for paging maintanance
//   memory    - pointer to the memory
//   size      - size of the memory
//   page_size - size of one page in the memory 
//   returns -  n number of pages in the memory
//           - -1 size is zero or not power of two
//           - -2 page_size is zero or not power of two or bigger then memory size
//           - -3 memory not zeroed or nullptr
int init_ram(void *memory, uint16_t size, uint8_t page_size);

// produces ram stats data to provided buffer
// returns  n number of bytes written to buffer if n >= size output was truncated
//         -1 error
// Format of the output in the buffer:
// free frames:            %u
// used frames by system:  %u
// used frames by tasks:   %u
// task frames
// %d   %u
// frames total:           %u
int dump_ram_stats(char *buffer, uint16_t size);

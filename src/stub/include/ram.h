#pragma once

#include <stdint.h>

typedef struct tRam
{
    uint16_t size;
    uint8_t page_size;
    uint8_t *bitmap;
} tRam;

// Init ram model. Library can use only this memory for paging maintenance.
// Size of a page is number of words. where word has a size 1 Byte.
//   memory    - pointer to the memory
//   size      - size of the memory
//   page_size - size of one page in the memory
//   returns -  n number of frames in the memory
//           - -1 size is zero or not power of two
//           - -2 page_size is zero or not power of two or bigger then memory size
//           - -3 memory not zeroed or nullptr
//           - -4 not enough memory to store size of tRam and bitmap
int init_ram(void *memory, uint16_t size, uint8_t page_size);

// Destroy ram model. Releases all resources.
void destroy_ram();

// Reserve specified number of consecutive frames in ram.
// This is low level utility that may be used by system.
// Uses first fit algorithm
//  returns - nullptr when there is no such free space present or ram is not initialized
//          - pointer to start of first reserved frame
void *falloc(uint16_t number);

// Free reserved number of consecutive frames in ram.
// This is low level utility that may be used by system. It may mark wrong frames as free without warning
void ffree(const void *memory, uint16_t number);

// Returns pointer to a tRam structure stored in the ram.
//  returns - pointer to tRam
//          - nullptr when ram is not initialized
const tRam *get_ram_state();

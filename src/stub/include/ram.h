#pragma once

#include <stdint.h>

typedef struct tRam
{
    uint16_t size;     // Configured size of RAM.
    uint8_t page_size; // Configured size of page.
    uint8_t *bitmap;   // Pointer to a RAM usage bitmap. Stored in RAM too.
} tRam;

// Initializes the RAM model. The library can use only this memory for task data,
// free space management, and paging maintenance.
// The size of a page is given in words, where one word equals 1 byte.
//
// Parameters:
//   memory    - Pointer to the memory that will represent the RAM.
//   size      - Total size of the memory.
//   page_size - Size of one page (frame) in the memory.
//
// Returns:
//    n   - Number of frames in the memory.
//   -1   - Size is zero or not a power of two.
//   -2   - Page size is zero, not a power of two, or larger than memory size.
//   -3   - Memory not zeroed or is nullptr.
//   -4   - Not enough memory to store tRam structure and bitmap.
int init_ram(void *memory, uint16_t size, uint8_t page_size);

// Destroys the RAM model and releases all associated resources in RAM.
void destroy_ram();

// Reserves the specified number of consecutive frames in RAM.
// This is a low-level utility that may be used by the system.
// Uses the first-fit algorithm.
//
// Parameters:
//   frame_id - Pointer to a variable that receives the ID of the first reserved frame.
//   number   - Number of frames to reserve.
//
// Returns:
//    0   - Success.
//   -1   - Not enough space or RAM is not initialized.
//   -2   - Invalid parameters.
int falloc(uint16_t *frame_id, uint16_t number);

// Frees a reserved number of consecutive frames in RAM.
// This is a low-level utility that may be used by the system.
// It may mark incorrect frames as free without warning.
// When the Ram is not initialized or frame_id is outside of the available RAM
// this function does nothing.
//
// Parameters:
//   frame_id - ID of the first frame to release.
//   number   - Number of frames to free.
void ffree(uint16_t frame_id, uint16_t number);

// Returns a pointer to the tRam structure stored in RAM.
//
// Returns:
//   Pointer to tRam.
//   nullptr if RAM is not initialized.
const tRam *get_ram_state();

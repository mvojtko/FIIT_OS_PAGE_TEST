#pragma once

#include <stdint.h>

#define PAGE_TABLE_SIZE 8

typedef struct tPageTableEntry
{
    uint8_t r : 1;  // Read access.
    uint8_t w : 1;  // Write access.
    uint8_t x : 1;  // Execute access.
    // rwx = 000 means the page is not available to the task; access to address within this page will cause a
    // segmentation fault.
    uint8_t p_bit : 1;  // Page is present in RAM.
    uint8_t r_bit : 1;  // Page has been referenced.
    uint8_t m_bit : 1;  // Page has been modified.
    uint16_t frame_id;  // Assigned frame in RAM if p_bit is set.
} tPageTableEntry;

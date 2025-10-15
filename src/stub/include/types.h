#pragma once

#include <stdint.h>

#define PAGE_TABLE_SIZE 8

typedef struct tPageTableEntry
{
    uint8_t r : 1;      // read access rwx 000 -> means the page is not accessible in task and access means seg-fault
    uint8_t w : 1;      // write access
    uint8_t x : 1;      // execute access
    uint8_t p_bit : 1;  // page present in ram
    uint8_t r_bit : 1;  // page was referenced
    uint8_t m_bit : 1;  // page modified
    uint16_t frame_id;  // assigned frame in ram
} tPageTableEntry;

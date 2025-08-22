#pragma once

#include <stdint.h>

typedef struct tPage_table_entry
{
    uint8_t r : 1;     // read access
    uint8_t w : 1;     // write access
    uint8_t x : 1;     // execute access
    uint8_t p_bit : 1; // page present in ram
    uint8_t r_bit : 1; // page was referenced
    uint8_t m_bit : 1; // page modified
    uint16_t frame_id;
} tPage_table_entry;

// function loads page content to ram
//   pid        - task identification
//   page_id    - page id loaded to ram
//   data       - page data loaded to ram
//   returns:  0 - success
//            -1 - out of page frames
//            -2 - segmentation fault
//            -3 - task not found
int load_page(int pid, uint16_t page_id, void *data);

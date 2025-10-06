#pragma once

#include <stdint.h>

// function loads page content to ram
//   pid        - task identification
//   page_id    - page id loaded to ram
//   data       - page data loaded to ram
//   returns:  0 - success
//            -1 - out of page frames
//            -2 - segmentation fault
//            -3 - task not found
int load_page(int pid, uint16_t page_id, const void *data);
int release_page(int pid, uint16_t page_id);
int store_page(int pid, uint16_t page_id);
int restore_page(int pid, uint16_t page_id);

#pragma once

#include <stdint.h>

// pager works with m_bit and r_bit of the page table entry.
// the algorithm has:
//   - behavior as described for NRU (not-recently-used) (4 classes)
//   - local scope i.e. it may select as victim only frames owned by the task
//   - respects task's max_frames setting if configured
//   - during page_fault execution any modified page of task is first written to task's address-space
//   - during page_fault execution r_bit and m_bit of all task's pages are reset

// function loads page content in tasks address_space to ram
//   pid        - task identification
//   virtual_address  - start of page that will be loaded to ram
//   returns:  0 - success
//            -1 - task not found
//            -2 - page already in ram
//            -3 - out of resources
//            -4 - segmentation fault
int page_fault(int pid, uint16_t virtual_address);

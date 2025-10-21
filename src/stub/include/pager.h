#pragma once

#include <stdint.h>

// The paging algorithm works with the m_bit and r_bit fields of the page table entry.
// The algorithm has the following properties:
//   - Behavior as described for the NRU (Not Recently Used) algorithm (4 classes).
//   - Local scope, i.e., it may select as a victim only frames owned by the task.
//   - Respects the task's max_frames setting if configured.
//   - During page_fault execution, all modified pages of the task are first written to the task's address space.
//   - During page_fault execution, the r_bit and m_bit of all the task's pages are cleared.

// Loads the page content from the task's address space into RAM.
//   pid              - Task identifier.
//   virtual_address  - Address of data with missing frame in the RAM.
//   Returns:  0  - Success
//            -1  - Task not found
//            -2  - Page already in RAM
//            -3  - Out of resources
//            -4  - Segmentation fault
int page_fault(int pid, uint16_t virtual_address);

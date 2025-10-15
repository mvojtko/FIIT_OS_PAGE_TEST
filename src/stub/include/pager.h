#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "types.h"

// pager works with m_bit and r_bit of the page table entry.
// the algorithm has:
//   - behavior as described for TODO (4 classes)
//   - local scope i.e. it may select as victim only frames owned by the task
//   - reflects task's max_frames configuration if configured
//   - modified page selected as victim is first written to task's address-space 

// function loads page content in tasks address_space to ram
//   pid        - task identification
//   virtual_address  - start of page that will be loaded to ram
//   returns:  0 - success
//            -1 - page already in ram
//            -2 - segmentation fault
int page_fault(int pid, uint16_t virtual_address);

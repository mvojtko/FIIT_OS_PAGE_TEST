#pragma once

#include <stdint.h>

#include "types.h"

// sets new active page_table to MMU (memory management unit)
//  page_table - handle to physical address of page_table
void set_page_table(tPageTableEntry *page_table);

// calculates physical_address from virtual_address
//   returns:  0 - on success and fills physical_address relative to start of ram memory location
//            -1 - page-fault
//            -2 - segmentation-fault
//            -3 - physical_address nullptr
//            -4 - no page_table present
//            -5 - ram not initialized
int get_physical_address(uint16_t virtual_address, uint16_t *physical_address);

// Instruction execution api functions of MMU
//   virtual_address - address where the data are in virtual address space
//   data - either pointer that will be filled with data from ram or data to be stored to ram
//   returns:  0 - success
//            -1 - page-fault
//            -2 - segmentation-fault
//            -3 - access violation
//            -4 - no page_table present

// load ram content at calculated physical address to data
int fetch_instruction(uint16_t virtual_address, uint8_t *data);
int load_data(uint16_t virtual_address, uint8_t *data);

// stores data to ram at calculated physical address.
int store_data(uint16_t virtual_address, uint8_t data);

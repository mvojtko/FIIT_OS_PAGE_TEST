#pragma once

#include <stdint.h>

#include "types.h"

// Sets a new active page table for the MMU (Memory Management Unit).
//   page_table - Pointer to the physical address of the page table.
void set_page_table(tPageTableEntry *page_table);

// Calculates the physical address corresponding to a virtual address.
//   Returns:  0  - Success; fills physical_address with the address relative to the start of RAM.
//            -1  - Page fault.
//            -2  - Segmentation fault.
//            -3  - physical_address is nullptr.
//            -4  - No page table present.
//            -5  - RAM not initialized.
int get_physical_address(uint16_t virtual_address, uint16_t *physical_address);

// Instruction execution and Data processing API functions of the MMU.
//   virtual_address - Address of the data in the virtual address space.
//   data            - Pointer to the buffer that will be filled with data from RAM,
//                     or a value to be stored in RAM.
//   Returns:  0  - Success.
//            -1  - Page fault.
//            -2  - Segmentation fault.
//            -3  - Access violation.
//            -4  - No page table present.

// Loads RAM content at the calculated physical address into data.
int fetch_instruction(uint16_t virtual_address, uint8_t *data);
int load_data(uint16_t virtual_address, uint8_t *data);

// Stores data to RAM at the calculated physical address.
int store_data(uint16_t virtual_address, uint8_t data);

#pragma once

#include <stdint.h>

// sets active page_table to memory management unit
//  page_table - handle to page_table
int set_page_table(void *page_table);

// loads instruction content.
// when succesful it sets r_bit in the found page_table_netry
//   virtual_address - address where the data are in virtual address space
//   data - either poiter that will be filled with data from ram or data to be stored to ram
//   returns:  0 - success
//            -1 - page-fault
//            -2 - access violation
//            -3 - no page_table present
int load_instruction(uint16_t virtual_address, uint8_t *data);

// loads data content.
// when succesful it sets r_bit in the found page_table_netry
//   virtual_address - address where the data are in virtual address space
//   data - either poiter that will be filled with data from ram or data to be stored to ram
//   returns:  0 - success
//            -1 - page-fault
//            -2 - access violation
//            -3 - no page_table present
int load_data(uint16_t virtual_address, uint8_t *data);

// loads data content.
// when succesful it sets r_bit and m_bit in the found page_table_netry
//   virtual_address - address where the data are in virtual address space
//   data - either poiter that will be filled with data from ram or data to be stored to ram
//   returns:  0 - success
//            -1 - page-fault
//            -2 - access violation
//            -3 - no page_table present
int store_data(uint16_t virtual_address, uint8_t data);

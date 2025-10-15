#include <stddef.h>

#include "mmu.h"
#include "ram.h"

static tPageTableEntry *g_page_table = NULL;

void set_page_table(tPageTableEntry *page_table)
{
    g_page_table = page_table;
}

int get_physical_address(uint16_t virtual_address, uint16_t *physical_address)
{
    if (physical_address == NULL)
        return -3;

    if (g_page_table == NULL)
        return -4;

    const tRam * ram = get_ram_state();
    if (ram == NULL)
        return -5;

    uint16_t offset_mask = ram->page_size - 1;
    uint16_t id = virtual_address / ram->page_size;

    if (id >= PAGE_TABLE_SIZE && g_page_table[id].r == 0x0 && g_page_table[id].w == 0x0 && g_page_table[id].x == 0x0)
        return -2;

    if (g_page_table[id].p_bit == 0)
        return -1;

    *physical_address = g_page_table[id].frame_id * ram->page_size + (virtual_address & offset_mask);
    return 0;
}

int fetch_instruction(uint16_t virtual_address, uint8_t *data)
{
    uint16_t phy = 0;
    int ret = get_physical_address(virtual_address, &phy);
    if (ret != 0)
        return ret;

    const tRam * ram = get_ram_state();
    uint16_t id = virtual_address / ram->page_size;
    if (g_page_table[id].x == 0x0)
        return -3;

    *data = ((uint8_t *)ram)[phy];
    return 0;
}
int load_data(uint16_t virtual_address, uint8_t *data)
{
    uint16_t phy = 0;
    int ret = get_physical_address(virtual_address, &phy);
    if (ret != 0)
        return ret;

    const tRam * ram = get_ram_state();
    uint16_t id = virtual_address / ram->page_size;
    if (g_page_table[id].r == 0x0)
        return -3;

    *data = ((uint8_t *)ram)[phy];
    return 0;
}
int store_data(uint16_t virtual_address, uint8_t data)
{
    uint16_t phy = 0;
    int ret = get_physical_address(virtual_address, &phy);
    if (ret != 0)
        return ret;

    const tRam * ram = get_ram_state();
    uint16_t id = virtual_address / ram->page_size;
    if (g_page_table[id].w == 0x0)
        return -3;

    ((uint8_t *)ram)[phy] = data;
    return 0;
}

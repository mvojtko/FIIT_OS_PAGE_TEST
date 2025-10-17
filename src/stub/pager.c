#include <string.h>

#include "pager.h"
#include "ram.h"
#include "task.h"
#include "types.h"


int page_fault(int pid, uint16_t virtual_address)
{
    const tRam *ram = get_ram_state();
    tTaskStruct *task = get_task_struct(pid);
    if (ram == NULL || task == NULL)
        return -1;

    const uint8_t size = ram->page_size;
    const uint16_t page_id = virtual_address / size;
    if (page_id >= PAGE_TABLE_SIZE)
        return -4;

    tPageTableEntry *entry = &task->page_table[page_id];
    if (entry->r == 0x0 && entry->w == 0x0 && entry->x == 0x0)
        return -4;

    if (entry->p_bit == 0x1)
        return -2;

    // TODO if the number of frames reaches max need to find victim
    uint8_t cnt = 0;
    uint8_t score = 0;
    uint8_t victim_id = 0;
    for (uint8_t id = 0; id < PAGE_TABLE_SIZE; id++)
    {
        tPageTableEntry *entry = &task->page_table[id];
        if (entry->p_bit == 0x0)
            continue;

        cnt++;
        if (score < 4 && entry->r_bit == 0x0 && entry->m_bit == 0x0)
        {
            score = 4;
            victim_id = id;
        }
        if (score < 3 && entry->r_bit == 0x0 && entry->m_bit == 0x1)
        {
            score = 3;
            victim_id = id;
        }
        if (score < 2 && entry->r_bit == 0x1 && entry->m_bit == 0x0)
        {
            score = 2;
            victim_id = id;
        }
        if (score < 1 && entry->r_bit == 0x1 && entry->m_bit == 0x1)
        {
            score = 1;
            victim_id = id;
        }
        if (entry->m_bit == 0x1)
        {
            memcpy((uint8_t *)task->address_space + (id * size), (uint8_t *)ram + (entry->frame_id * size), size);
        }
        entry->r_bit = 0x0;
        entry->m_bit = 0x0;
    }

    if ((task->max_frames != 0 && cnt == task->max_frames) || falloc(&entry->frame_id, 1) != 0)
    {
        if (cnt == 0)  // this means we have no frames present and falloc failed
        {
            return -3;
        }
        tPageTableEntry *victim = &task->page_table[victim_id];
        entry->frame_id = victim->frame_id;
        victim->frame_id = 0;
        victim->p_bit = 0x0;
    }

    entry->p_bit = 0x1;
    memcpy((uint8_t *)ram + (entry->frame_id * size), (uint8_t *)task->address_space + (page_id * size), size);

    return 0;
}

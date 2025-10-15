#include <string.h>

#include "pager.h"
#include "ram.h"
#include "task.h"
#include "types.h"


int page_fault(int pid, uint16_t virtual_address)
{
    const tRam *ram = get_ram_state();
    tTaskStruct *task = get_task_Struct(pid);
    if (ram == NULL || task == NULL)
        return -1;

    uint16_t page_id = virtual_address / ram->page_size;
    if (page_id >= PAGE_TABLE_SIZE)
        return -3;

    tPageTableEntry *entry = &task->page_table[page_id];
    if (entry->r == 0x0 && entry->w == 0x0 && entry->x == 0x0)
        return -1;

    void *frame = falloc(1);

    if (frame == NULL)
        return -1;

    memcpy(frame, (uint8_t *)task->address_space + (page_id * ram->page_size), ram->page_size);
    entry->frame_id = ((uint8_t *)frame - (uint8_t *)ram) / ram->page_size;
    return 0;
}

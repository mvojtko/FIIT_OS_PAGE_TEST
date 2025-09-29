#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "ram.h"

static tRam *g_ram = NULL;

#define NUM_RAM_FRAMES g_ram->size / g_ram->page_size
#define NUM_FRAMES(bytes) ((bytes) / g_ram->page_size) + (((bytes) % g_ram->page_size) ? 1 : 0)

uint8_t *init_bitmap()
{
    if (NUM_FRAMES(sizeof(tRam)) > NUM_RAM_FRAMES)
        return NULL;

    // number of bytes of tRam data with bitmap
    uint16_t bytes = sizeof(tRam) + ((NUM_RAM_FRAMES >= 8) ? (NUM_RAM_FRAMES / 8) : 1);
    uint16_t frames = NUM_FRAMES(bytes);

    if (frames > NUM_RAM_FRAMES)
        return NULL;

    g_ram->bitmap = (uint8_t *)(g_ram + 1);
    for (uint16_t frame_id = 0; frame_id < frames; frame_id++)
    {
        g_ram->bitmap[frame_id / 8] |= (0x01 << (frame_id % 8));
    }
    return g_ram->bitmap;
}

int init_ram(void *memory, uint16_t size, uint8_t page_size)
{
    if (NULL == memory)
        return -3;

    if (size == 0 || (size & (size -1)))
    {
        return -1;
    }

    if (page_size == 0 || page_size > size || (page_size & (page_size -1)))
    {
        return -2;
    }

    for (uint16_t pos = 0; pos < size; pos++)
    {
        if (((char *)memory)[pos] != 0)
            return -3;
    }

    g_ram = (tRam *)memory;
    g_ram->size = size;
    g_ram->page_size = page_size;
    if (init_bitmap())
    {
        return size / page_size;
    }

    g_ram->size = 0;
    g_ram->page_size = 0;
    g_ram = NULL;
    return -4;
}

void destroy_ram()
{
    g_ram = NULL;
}

const tRam *get_ram_state()
{
    return g_ram;
}

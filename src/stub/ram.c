#include "pager.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

static void *g_memory = NULL;
static uint16_t g_size = 0;
static uint8_t g_page_size = 0;

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

    g_memory = memory;
    g_size = size;
    g_page_size = page_size;
    return 0;
}

int dump_ram_stats(char *buffer, uint16_t size)
{
    return snprintf(buffer, size,
        "free frames:            %u\n"
        "used frames by system:  %u\n"
        "used frames by tasks:   %u\n"
        "frames total:           %u\n", g_size, 0 , 0, g_size);
}

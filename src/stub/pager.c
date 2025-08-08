#include "pager.h"
#include <stddef.h>
#include <string.h>

static void *g_memory = NULL;
static uint16_t g_size = 0;
static uint8_t g_page_size = 0;

int init_ram(void *memory, uint16_t size, uint8_t page_size)
{
    memset(memory, 0, size);
    g_memory = memory;
    g_size = size;
    g_page_size = page_size;
    return 0;
}

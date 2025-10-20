#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#include "debug.h"

static uint8_t g_debug = 0;

void set_debug()
{
    g_debug = 1;
}

int dprintf(const char *fmt, ...)
{
    if (g_debug == 0)
        return 0;
    va_list args;
    va_start(args, fmt);

    int ret = vfprintf(stderr, fmt, args);

    va_end(args);
    return ret;
}
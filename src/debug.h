#pragma once

#define NUMS_ELEMENTS(array) (sizeof(array) / sizeof(array[0]))

void set_debug();
int dprintf(const char *fmt, ...);

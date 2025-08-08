#include <stdio.h>
#include <stdlib.h>

#include "pager.h"

int main(int argc, char* argv[])
{
    printf("Starting Test\n");

    for (int index = 0; index < argc; index++)
    {
        printf("Argument %d: %s\n", index, argv[index]);
    }

    void * memory = malloc(1000);

    return init_ram(memory, 1024, 128);
}

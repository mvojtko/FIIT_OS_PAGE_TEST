#include <getopt.h>

#include "debug.h"
#include "gtest/gtest.h"

int main(int argc, char **argv)
{
    int opt = 0;
    testing::InitGoogleTest(&argc, argv);

    while ((opt = getopt(argc, argv, "d")) != -1)
    {
        switch (opt)
        {
            case 'd':
                set_debug();
                break;
            default:
                fprintf(stderr, "unknown argument!\n");
                return 1;
        }
    }

    return RUN_ALL_TESTS();
}
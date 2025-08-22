#include "debug.h"
#include "gtest/gtest.h"

extern "C" {
#include "ram.h"
}

TEST(init_ram, memory_nullptr)
{
    EXPECT_EQ(-3, init_ram(nullptr, 0, 0));
    EXPECT_EQ(-3, init_ram(nullptr, 1, 0));
    EXPECT_EQ(-3, init_ram(nullptr, 0, 1));
    EXPECT_EQ(-3, init_ram(nullptr, 1, 1));
}

TEST(init_ram, memory_not_zeroed)
{
    uint16_t mem[UINT16_MAX] = {1};
    EXPECT_EQ(-3, init_ram(mem, 1, 1));
    EXPECT_EQ(-3, init_ram(mem, 1 << 15, 1));
}

TEST(init_ram, size_invalid)
{
    int a = 0;
    EXPECT_EQ(-1, init_ram(&a, 0, 0));
    EXPECT_EQ(-1, init_ram(&a, 0, 1));
    EXPECT_EQ(-1, init_ram(&a, 3, 0));
    EXPECT_EQ(-1, init_ram(&a, 3, 1));
    uint16_t size = 1;
    while (size != 0)
    {
        if ((size) & (size - 1))
        {
            EXPECT_EQ(-1, init_ram(&a, size, 0));
        }
        size++; 
    }
}

TEST(init_ram, page_size_invalid)
{
    int a = 0;
    EXPECT_EQ(-2, init_ram(&a, 4, 0));
    EXPECT_EQ(-2, init_ram(&a, 4, 3));
    EXPECT_EQ(-2, init_ram(&a, 4, 5));
    EXPECT_EQ(-2, init_ram(&a, 4, 8));
    uint8_t size = 1;
    while (size != 0)
    {
        if ((size) & (size - 1))
        {
            EXPECT_EQ(-2, init_ram(&a, 256, size));
        }
        size++; 
    }
}

TEST(dump_ram_stats, buffer_nullptr)
{
    EXPECT_EQ(130, dump_ram_stats(nullptr, 0));
}

TEST(dump_ram_stats, zero_size)
{
    char buffer = 0;
    EXPECT_EQ(130, dump_ram_stats(&buffer, 0));
    EXPECT_EQ(0, buffer);
}

TEST(dump_ram_stats, no_init_ram_short_buffer)
{
    char buffer[10] = { 0 };
    EXPECT_EQ(130, dump_ram_stats(buffer, 1));
    EXPECT_STREQ("", buffer);
    EXPECT_EQ(130, dump_ram_stats(buffer, 2));
    EXPECT_STREQ("f", buffer);
    EXPECT_EQ(130, dump_ram_stats(buffer, 5));
    EXPECT_STREQ("free", buffer);
}

TEST(dump_ram_stats, no_init_ram_nearly_enough)
{
    char buffer[130] = { 0 };
    EXPECT_EQ(130, dump_ram_stats(buffer, 130));
    EXPECT_STREQ("free frames:            0\n"
                 "used frames by system:  0\n"
                 "used frames by tasks:   0\n"
                 "frames total:           0\n"
                 "memory total:           0", buffer);
}

TEST(dump_ram_stats, no_init_ram_sufficient_buffer)
{
    char buffer[131] = { 0 };
    EXPECT_EQ(130, dump_ram_stats(buffer, 131));
    EXPECT_STREQ("free frames:            0\n"
                 "used frames by system:  0\n"
                 "used frames by tasks:   0\n"
                 "frames total:           0\n"
                 "memory total:           0\n", buffer);
}

TEST(dump_ram_stats, init_ram)
{
    #define BUF_LEN 200
    uint16_t mem[UINT16_MAX] = {0};
    for (uint16_t size = 1; size != 0; size <<= 1 )
    {
        for (uint8_t page_size = 1; page_size <= size && page_size != 0; page_size <<= 1 )
        {
            dprintf("size %u, page_size %u\n", size, page_size);
            char exp[BUF_LEN] = { 0 };
            snprintf(exp, BUF_LEN, 
                     "free frames:            %u\n"
                     "used frames by system:  %u\n"
                     "used frames by tasks:   %u\n"
                     "frames total:           %u\n"
                     "memory total:           %u\n", size / page_size, 0, 0, size / page_size, size);
            EXPECT_EQ(0, init_ram(mem, size, page_size));
            char actual[BUF_LEN] = { 0 };
            dump_ram_stats(actual, BUF_LEN);
            EXPECT_STREQ(exp, actual);
        }
    }
}

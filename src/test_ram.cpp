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
    int a = 1;
    EXPECT_EQ(-3, init_ram(&a, 1, 1));
}

TEST(init_ram, memory_size_invalid)
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
}

TEST(dump_ram_stats, buffer_nullptr)
{
    EXPECT_EQ(104, dump_ram_stats(nullptr, 0));
}

TEST(dump_ram_stats, zero_size)
{
    char buffer = 0;
    EXPECT_EQ(104, dump_ram_stats(&buffer, 0));
    EXPECT_EQ(0, buffer);
}

TEST(dump_ram_stats, no_init_ram_short_buffer)
{
    char buffer[10] = { 0 };
    EXPECT_EQ(104, dump_ram_stats(buffer, 1));
    EXPECT_STREQ("", buffer);
    EXPECT_EQ(104, dump_ram_stats(buffer, 2));
    EXPECT_STREQ("f", buffer);
    EXPECT_EQ(104, dump_ram_stats(buffer, 5));
    EXPECT_STREQ("free", buffer);
}

TEST(dump_ram_stats, no_init_ram_nearly_enough)
{
    char buffer[104] = { 0 };
    EXPECT_EQ(104, dump_ram_stats(buffer, 104));
    EXPECT_STREQ("free frames:            0\n"
                 "used frames by system:  0\n"
                 "used frames by tasks:   0\n"
                 "frames total:           0", buffer);

}

TEST(dump_ram_stats, no_init_ram_sufficient_buffer)
{
    char buffer[105] = { 0 };
    EXPECT_EQ(104, dump_ram_stats(buffer, 105));
    EXPECT_STREQ("free frames:            0\n"
                 "used frames by system:  0\n"
                 "used frames by tasks:   0\n"
                 "frames total:           0\n", buffer);
}

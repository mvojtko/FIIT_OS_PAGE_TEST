#include "gtest/gtest.h"
#include "debug.h"
#include "test_ram.h"

extern "C" {
#include "ram.h"
}

static constexpr size_t kBufferSize = 32768;  // large enough for max-size tests

class InitRamTest : public ::testing::Test
{
  protected:
    ~InitRamTest()
    {
        destroy_ram();
    }

    void SetUp() override
    {
        memset(buffer, 0, sizeof(buffer));  // ensure zeroed before each test
    }

    uint8_t buffer[kBufferSize];
};

// --- init_ram tests ---
TEST_F(InitRamTest, MemoryNotZeroed)
{
    buffer[0] = 1;
    int ret = init_ram(buffer, 256, 16);
    EXPECT_EQ(ret, -3);
}

TEST_F(InitRamTest, NullMemoryPointer)
{
    int ret = init_ram(nullptr, 256, 16);
    EXPECT_EQ(ret, -3);
}

TEST_F(InitRamTest, AllInvalidParams)
{
    int ret = init_ram(nullptr, 0, 0);
    EXPECT_LE(ret, -1);
    ret = init_ram(nullptr, 3, 3);
    EXPECT_LE(ret, -1);
}

TEST_F(InitRamTest, GetRamStateNullBeforeInit)
{
    const tRam *ret = get_ram_state();
    EXPECT_EQ(nullptr, ret);
}

TEST_F(InitRamTest, GetRamStateNullAfterDestroyRam)
{
    init_ram(buffer, 256, 32);
    const tRam *ret = get_ram_state();
    EXPECT_NE(nullptr, ret);
    destroy_ram();
    ret = get_ram_state();
    EXPECT_EQ(nullptr, ret);
}

// --- Parameterized tests ---
struct InitRamCase
{
    uint16_t size;
    uint8_t page_size;
    int expected;
    std::string name;
};

std::string PrintToString(InitRamCase item)
{
    return std::to_string(item.size) + "_" + std::to_string(item.page_size) + "_" + item.name;
}

class RamParamTest : public ::testing::TestWithParam<InitRamCase>
{
  protected:
    ~RamParamTest()
    {
        destroy_ram();
    }

    void SetUp() override
    {
        memset(buffer, 0, sizeof(buffer));
    }

    uint8_t buffer[kBufferSize];
};

uint8_t reversByte(uint8_t b)
{
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

uint16_t getOccupiedFrames(const tRam &ram, bool *isContinuous)
{
    uint8_t *bitmap = ram.bitmap;
    uint16_t num_frames = ram.size / ram.page_size;
    uint16_t bitmapSize = (num_frames > 8) ? num_frames / 8 : 1;

    if (bitmap == nullptr)
        return 0;

    // check frames used by system
    uint16_t occupiedFrames = 0;
    bool blockStarted = false;
    bool blockEnded = false;
    dprintf("Ram frames bitmask:");
    for (uint16_t byte_id = 0; byte_id < bitmapSize; byte_id++)
    {
        uint8_t byte = bitmap[byte_id];

        if ((byte_id % 64) == 0)
        {
            dprintf("\n0x%03x ->", byte_id);
        }
        if ((byte_id % 4) == 0)
            dprintf(" ");
        dprintf("%02x", reversByte(byte));

        for (uint8_t bit = 0x01; bit != 0x00; bit <<= 1)
        {
            if (byte & bit)
            {
                blockStarted = true;
                occupiedFrames++;
                if (isContinuous != NULL && blockStarted && blockEnded)
                {
                    *isContinuous = false;
                }
            }
            else if (blockStarted)
            {
                blockEnded = true;
            }
        }
    }
    dprintf("\n");
    return occupiedFrames;
}

uint16_t RamTestBase::getOccupiedFrames(bool *isContinuous)
{
    const tRam *ret = get_ram_state();
    if (ret)
        return ::getOccupiedFrames(*ret, isContinuous);

    return 0;
}

void RamTestBase::dumpRamState()
{
    const tRam *ram = get_ram_state();
    if (ram == NULL)
        return;

    dprintf("Ram content:");
    for (uint16_t byte_id = 0; byte_id < ram->size; byte_id++)
    {
        uint8_t byte = ((const uint8_t *)ram)[byte_id];

        if ((byte_id % 64) == 0)
        {
            dprintf("\n0x%03x ->", byte_id);
        }
        if ((byte_id % 4) == 0)
            dprintf(" ");
        dprintf("%02x", byte);
    }
    dprintf("\n");
}

TEST_P(RamParamTest, InitRam)
{
    auto p = GetParam();
    int result = init_ram(buffer, p.size, p.page_size);
    EXPECT_EQ(result, p.expected);
}

TEST_P(RamParamTest, GetRamState)
{
    auto p = GetParam();
    int result = init_ram(buffer, p.size, p.page_size);
    const tRam *ret = get_ram_state();
    if (result <= 0)
        return;

    ASSERT_NE(ret, nullptr);

    // test that there is some ram already used by system
    uint16_t bitmapSize = (result > 8) ? (result / 8) : 1;
    uint16_t occupiedBytes = bitmapSize + sizeof(tRam);
    uint16_t expectedOccupiedFrames = (occupiedBytes / p.page_size) + ((occupiedBytes % p.page_size) ? 1 : 0);

    bool isContinuous = true;
    uint16_t occupiedFrames = getOccupiedFrames(*ret, &isContinuous);

    EXPECT_EQ(occupiedFrames, expectedOccupiedFrames);
    EXPECT_TRUE(isContinuous);
}

TEST_P(RamParamTest, GetRamState_boundaries)
{
    auto p = GetParam();
    int result = init_ram(buffer, p.size, p.page_size);
    const tRam *ret = get_ram_state();
    if (result > 0)
    {
        ASSERT_NE(ret, nullptr);

        // returned sizes are equal to input sizes
        EXPECT_EQ(ret->size, p.size);
        EXPECT_EQ(ret->page_size, p.page_size);
        EXPECT_EQ(result, ret->size / ret->page_size);
        // returned struct is inside reserved memory
        EXPECT_GE((uint8_t *)ret, buffer);
        EXPECT_LT((uint8_t *)ret, buffer + p.size);
        // bitmap is inside reserved ram memory
        EXPECT_GE(ret->bitmap, buffer);
        EXPECT_GE(ret->bitmap, buffer + sizeof(tRam));
        EXPECT_LT(ret->bitmap, buffer + p.size);
    }
    else
    {
        EXPECT_EQ(nullptr, ret);
    }
}

uint16_t nextPow2(uint16_t num)
{
    uint16_t pow2 = 0x1;
    for (; pow2 <= num; pow2 <<= 0x1)
        ;

    return pow2;
}

INSTANTIATE_TEST_SUITE_P(Negative,
    RamParamTest,
    ::testing::Values(InitRamCase{0, 16, -1, "ZeroSize"},
        InitRamCase{256, 0, -2, "ZeroPageSize"},
        InitRamCase{300, 16, -1, "InvalidSize"},
        InitRamCase{256, 24, -2, "InvalidPageSize"},
        InitRamCase{64, 128, -2, "ValidSizeSmallerThanValidPageSize"},
        InitRamCase{16, 16, -4, "ValidSizeNotEnoughResources"}),
    testing::PrintToStringParamName());

INSTANTIATE_TEST_SUITE_P(Positive,
    RamParamTest,
    ::testing::Values(InitRamCase{256, 16, 16, "NormalValidCase"},
        InitRamCase{nextPow2(sizeof(tRam)), 1, nextPow2(sizeof(tRam)), "MinValidSizeMinPageSize"},
        InitRamCase{nextPow2(sizeof(tRam)), (uint8_t)nextPow2(sizeof(tRam)), 1, "MinValidSizeMaxPageSize"},
        InitRamCase{32768, 1, 32768, "MaxValidSizeMinPageSize"},
        InitRamCase{32768, 128, 256, "MaxValidSizeMaxPageSize"}),
    testing::PrintToStringParamName());

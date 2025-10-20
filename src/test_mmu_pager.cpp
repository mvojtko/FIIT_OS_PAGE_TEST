#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#include <cstdint>
#include <cstring>
#include <vector>

#include "gtest/gtest.h"
#include "test_ram.h"

extern "C" {
#include "mmu.h"
#include "ram.h"
#include "pager.h"
#include "task.h"
}

#define CODE_PAGE(page_id) ((page_id % 8) < 3)
#define DATA_PAGE(page_id) ((page_id % 8) >= 2)
#define WRITE_PAGE(page_id) ((page_id % 8) > 3)

constexpr int OK = 0;
constexpr int PAGE_FAULT = -1;
constexpr int ACCESS_VIOLATION = -3;
constexpr int OUT_OF_RESOURCES = -3;

class MMUPagerTest : public RamTestBase
{
  protected:
    static constexpr uint16_t OFFSET_MASK = PAGE_SIZE - 1;

    void SetUp() override
    {
        int ret = init_taskMgr();
        ASSERT_EQ(ret, 0);

        tPageTableEntry table[PAGE_TABLE_SIZE];
        memset(table, 0, sizeof(tPageTableEntry) * PAGE_TABLE_SIZE);
        for (uint8_t id = 0; id < PAGE_TABLE_SIZE; id++)
        {
            memset(address_space + PAGE_SIZE * id, id + 5, PAGE_SIZE);
            table[id].r = (DATA_PAGE(id)) ? 0x1 : 0x0;
            table[id].w = (WRITE_PAGE(id)) ? 0x1 : 0x0;
            table[id].x = (CODE_PAGE(id)) ? 0x1 : 0x0;
        }

        ret = create_task(table, 3, address_space);
        ASSERT_GE(ret, 0);
        task = get_task_struct(ret);
        ASSERT_NE(task, nullptr);
        set_page_table(task->page_table);
    }

    void TearDown() override
    {
        destroy_taskMgr();
        set_page_table(nullptr);
    }

    uint8_t address_space[PAGE_SIZE * PAGE_TABLE_SIZE];
    tTaskStruct *task;
};

TEST_F(MMUPagerTest, GetPhysicalAddressSuccessAfterPageFault)
{
    uint16_t phys = 0;
    uint16_t addr = PAGE_SIZE * 1 + 15;
    int page_fault_res = page_fault(task->pid, addr);
    ASSERT_EQ(page_fault_res, OK);
    int get_physical_address_res = get_physical_address(addr, &phys);
    EXPECT_EQ(get_physical_address_res, 0);
    uint16_t frame = task->page_table[1].frame_id * PAGE_SIZE;
    EXPECT_EQ(phys, frame + (addr & OFFSET_MASK));
    EXPECT_EQ(task->page_table[1].p_bit, 0x1); 
}

TEST_F(MMUPagerTest, FetchInstructionSuccessAfterPageFault)
{
    uint8_t instr = 0;
    uint16_t addr = PAGE_SIZE * 2 + 33;
    int page_fault_res = page_fault(task->pid, addr);
    ASSERT_EQ(page_fault_res, OK);

    int fetch_instruction_res = fetch_instruction(addr, &instr);
    EXPECT_EQ(fetch_instruction_res, 0);
    uint8_t instr_exp = address_space[addr];
    EXPECT_EQ(instr, instr_exp);
    uint8_t instr_ram = ((uint8_t *)ram)[task->page_table[2].frame_id * PAGE_SIZE];
    EXPECT_EQ(instr_ram, instr_exp);
    EXPECT_EQ(task->page_table[2].r_bit, 0x1);
    EXPECT_EQ(task->page_table[2].p_bit, 0x1);    
}

TEST_F(MMUPagerTest, LoadDataSuccessAfterPageFault)
{
    uint8_t data = 0;
    uint16_t addr = PAGE_SIZE * 4 + 55;
    int page_fault_res = page_fault(task->pid, addr);
    ASSERT_EQ(page_fault_res, OK);

    int load_data_res = load_data(addr, &data);
    EXPECT_EQ(load_data_res, 0);
    uint8_t data_exp = address_space[addr];
    EXPECT_EQ(data, data_exp);
    uint8_t data_ram = ((uint8_t *)ram)[task->page_table[4].frame_id * PAGE_SIZE + (addr & OFFSET_MASK)];
    EXPECT_EQ(data_ram, data_exp);
    EXPECT_EQ(task->page_table[4].r_bit, 0x1);
    EXPECT_EQ(task->page_table[4].p_bit, 0x1);    
}

TEST_F(MMUPagerTest, StoreDataSuccessAfterPageFault)
{
    uint16_t addr = PAGE_SIZE * 4 + 55;
    int page_fault_res = page_fault(task->pid, addr);
    ASSERT_EQ(page_fault_res, OK);

    int store_data_res = store_data(addr, 55);
    EXPECT_EQ(store_data_res, 0);
    uint8_t data_exp = address_space[addr];
    EXPECT_NE(55, data_exp);
    uint8_t data_ram = ((uint8_t *)ram)[task->page_table[4].frame_id * PAGE_SIZE + (addr & OFFSET_MASK)];
    EXPECT_EQ(55, data_ram);
    EXPECT_EQ(task->page_table[4].r_bit, 0x1);
    EXPECT_EQ(task->page_table[4].m_bit, 0x1);
    EXPECT_EQ(task->page_table[4].p_bit, 0x1);    
}

TEST_F(MMUPagerTest, StoreDataWrittenToAddressSpaceAfterPageFault)
{
    uint16_t addr = PAGE_SIZE * 4 + 55;
    int page_fault_res = page_fault(task->pid, addr);
    ASSERT_EQ(page_fault_res, OK);

    int store_data_res = store_data(addr, 55);
    ASSERT_EQ(store_data_res, 0);

    uint16_t addr2 = addr + PAGE_SIZE;
    page_fault_res = page_fault(task->pid, addr2);
    ASSERT_EQ(page_fault_res, OK);

    uint8_t data_exp = address_space[addr];
    EXPECT_EQ(55, data_exp);
    uint8_t data_ram = ((uint8_t *)ram)[task->page_table[4].frame_id * PAGE_SIZE + (addr & OFFSET_MASK)];
    EXPECT_EQ(data_ram, data_exp);
    EXPECT_EQ(task->page_table[4].r_bit, 0x0);
    EXPECT_EQ(task->page_table[4].m_bit, 0x0);
    EXPECT_EQ(task->page_table[4].p_bit, 0x1);    
}

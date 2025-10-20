#include "gtest/gtest.h"

#include "debug.h"
#include "test_ram.h"

extern "C" {
#include "mmu.h"
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

class Scale : public RamTestBase
{
  protected:
    void SetUp() override
    {
        int ret = init_taskMgr();
        ASSERT_EQ(ret, 0);
        memset(page_tables, 0, sizeof(tPageTableEntry) * TASK_TABLE_SIZE * PAGE_TABLE_SIZE);
        for (uint8_t id = 0; id < TASK_TABLE_SIZE * PAGE_TABLE_SIZE; id++)
        {
            memset(address_spaces + PAGE_SIZE * id, id, PAGE_SIZE);
            page_tables[id].r = (DATA_PAGE(id)) ? 0x1 : 0x0;
            page_tables[id].w = (WRITE_PAGE(id)) ? 0x1 : 0x0;
            page_tables[id].x = (CODE_PAGE(id)) ? 0x1 : 0x0;
        }
    }

    void TearDown() override
    {
        destroy_taskMgr();
        set_page_table(nullptr);
    }

    void LoadTasks()
    {
        for (uint8_t id = 0; id < TASK_TABLE_SIZE; id++)
        {
            int ret =
                create_task(page_tables + id * PAGE_TABLE_SIZE, id, address_spaces + id * PAGE_SIZE * PAGE_TABLE_SIZE);
            ASSERT_GE(ret, 0);
            dprintf("LoadTasks: created task pid %d\n", ret);
            tasks[id] = ret;
        }
        dumpRamState();
    }

    void LoadOnePage()
    {
        for (uint8_t id = 0; id < TASK_TABLE_SIZE; id++)
        {
            uint16_t address = PAGE_SIZE / 2;
            auto *task = get_task_struct(tasks[id]);
            ASSERT_NE(task, nullptr);
            int page_fault_res = page_fault(task->pid, address);
            ASSERT_EQ(page_fault_res, OK);
            ASSERT_EQ(task->page_table[0].p_bit, 0x1);
            dprintf("LoadPage0: for task pid %d\n", tasks[id]);
        }
        dumpRamState();
    }

    tPageTableEntry page_tables[TASK_TABLE_SIZE * PAGE_TABLE_SIZE];
    uint8_t address_spaces[TASK_TABLE_SIZE * PAGE_SIZE * PAGE_TABLE_SIZE];
    int tasks[TASK_TABLE_SIZE];
};

TEST_F(Scale, CreateMaxTasks)
{
    // create TASK_TABLE_SIZE tasks
    for (uint8_t id = 0; id < TASK_TABLE_SIZE; id++)
    {
        int ret =
            create_task(page_tables + id * PAGE_TABLE_SIZE, id, address_spaces + id * PAGE_SIZE * PAGE_TABLE_SIZE);
        EXPECT_GE(ret, 0);
        tasks[id] = ret;
    }
}

TEST_F(Scale, CheckNoFramesLoaded)
{
    LoadTasks();
    // any call to any address should be page_fault or violation
    for (uint8_t id = TASK_TABLE_SIZE; id > 0; id--)
    {
        auto *task = get_task_struct(tasks[id - 1]);
        EXPECT_NE(task, nullptr);
        set_page_table(task->page_table);
        for (uint8_t page_id = 0; page_id < PAGE_TABLE_SIZE; page_id++)
        {
            uint16_t address = page_id * PAGE_SIZE + PAGE_SIZE / 2;
            uint8_t data = 0;
            int fetch_instruction_res = fetch_instruction(address, &data);
            if (CODE_PAGE(page_id))
            {
                EXPECT_EQ(fetch_instruction_res, PAGE_FAULT);
            }
            else
            {
                EXPECT_EQ(fetch_instruction_res, ACCESS_VIOLATION);
            }
            int load_data_res = load_data(address, &data);
            if (DATA_PAGE(page_id))
            {
                EXPECT_EQ(load_data_res, PAGE_FAULT);
            }
            else
            {
                EXPECT_EQ(load_data_res, ACCESS_VIOLATION);
            }
            int store_data_res = store_data(address, 0xdd);
            if (WRITE_PAGE(page_id))
            {
                EXPECT_EQ(store_data_res, PAGE_FAULT);
            }
            else
            {
                EXPECT_EQ(store_data_res, ACCESS_VIOLATION);
            }
            EXPECT_EQ(data, 0);
        }
    }
}

TEST_F(Scale, CallPageFaultOnEachPage)
{
    LoadTasks();
    for (uint8_t id = TASK_TABLE_SIZE; id > 0; id--)
    {
        auto *task = get_task_struct(tasks[id - 1]);
        ASSERT_NE(task, nullptr);
        set_page_table(task->page_table);
        for (uint8_t page_id = 0; page_id < PAGE_TABLE_SIZE; page_id++)
        {
            uint8_t task_stored_data =
                address_spaces[(task->pid * PAGE_TABLE_SIZE + page_id) * PAGE_SIZE + PAGE_SIZE / 2];
            uint16_t address = page_id * PAGE_SIZE + PAGE_SIZE / 2;
            uint8_t data = 0;
            dprintf("Testing: task pid %d page %u\n", task->pid, page_id);

            int page_fault_res = page_fault(task->pid, address);
            if (page_id == 0 && getOccupiedFrames(nullptr) == NUM_FRAMES)
            {
                EXPECT_EQ(page_fault_res, OUT_OF_RESOURCES);
                break;
            }
            else
            {
                dumpRamState();
                ASSERT_EQ(page_fault_res, OK);
                ASSERT_EQ(task->page_table[page_id].p_bit, 0x1);
                int fetch_instruction_res = fetch_instruction(address, &data);
                if (CODE_PAGE(page_id))
                {
                    EXPECT_EQ(fetch_instruction_res, OK);
                    EXPECT_EQ(data, task_stored_data);
                    EXPECT_EQ(task->page_table[page_id].r_bit, 0x1);
                }
                else
                {
                    EXPECT_EQ(fetch_instruction_res, ACCESS_VIOLATION);
                }
                int load_data_res = load_data(address, &data);
                if (DATA_PAGE(page_id))
                {
                    EXPECT_EQ(load_data_res, OK);
                    EXPECT_EQ(data, task_stored_data);
                    EXPECT_EQ(task->page_table[page_id].r_bit, 0x1);
                }
                else
                {
                    EXPECT_EQ(load_data_res, ACCESS_VIOLATION);
                }
                int store_data_res = store_data(address, 0xdd);
                if (WRITE_PAGE(page_id))
                {
                    EXPECT_EQ(store_data_res, OK);
                    EXPECT_EQ(task->page_table[page_id].m_bit, 0x1);
                    EXPECT_EQ(task->page_table[page_id].r_bit, 0x1);
                }
                else
                {
                    EXPECT_EQ(store_data_res, ACCESS_VIOLATION);
                }
            }
        }
    }
}

TEST_F(Scale, CallPageFaultWithAllTasksHavingOnePageInRam)
{
    LoadTasks();
    LoadOnePage();
    for (uint8_t id = TASK_TABLE_SIZE; id > 0; id--)
    {
        auto *task = get_task_struct(tasks[id - 1]);

        ASSERT_NE(task, nullptr);
        set_page_table(task->page_table);
        for (uint8_t page_id = 1; page_id < PAGE_TABLE_SIZE; page_id++)
        {
            dprintf("Testing: task pid %d page %u\n", task->pid, page_id);
            uint8_t task_stored_data =
                address_spaces[(task->pid * PAGE_TABLE_SIZE + page_id) * PAGE_SIZE + PAGE_SIZE / 2];
            uint16_t address = page_id * PAGE_SIZE + PAGE_SIZE / 2;
            uint8_t data = 0;
            int page_fault_res = page_fault(task->pid, address);
            ASSERT_EQ(page_fault_res, OK);
            dumpRamState();
            int fetch_instruction_res = fetch_instruction(address, &data);
            if (CODE_PAGE(page_id))
            {
                EXPECT_EQ(fetch_instruction_res, OK);
                EXPECT_EQ(data, task_stored_data);
                EXPECT_EQ(task->page_table[page_id].r_bit, 0x1);
            }
            else
            {
                EXPECT_EQ(fetch_instruction_res, ACCESS_VIOLATION);
            }
            int load_data_res = load_data(address, &data);
            if (DATA_PAGE(page_id))
            {
                EXPECT_EQ(load_data_res, OK);
                EXPECT_EQ(data, task_stored_data);
                EXPECT_EQ(task->page_table[page_id].r_bit, 0x1);
            }
            else
            {
                EXPECT_EQ(load_data_res, ACCESS_VIOLATION);
            }
            int store_data_res = store_data(address, 0xdd);
            if (WRITE_PAGE(page_id))
            {
                uint8_t ram_stored_data =
                    ((uint8_t *)ram)[task->page_table[page_id].frame_id * PAGE_SIZE + PAGE_SIZE / 2];
                EXPECT_EQ(store_data_res, OK);
                EXPECT_EQ(0xdd, ram_stored_data);
                EXPECT_NE(task_stored_data, ram_stored_data)
                    << "store_data writes new value to ram and this is not reflected in address_space until written";
                EXPECT_EQ(task->page_table[page_id].m_bit, 0x1);
                EXPECT_EQ(task->page_table[page_id].r_bit, 0x1);
            }
            else
            {
                EXPECT_EQ(store_data_res, ACCESS_VIOLATION);
            }
        }
    }
}

TEST_F(Scale, CallPageFaultWithAllTasksHavingOnePageInRamModifiedWrittenToAddressSpace)
{
    LoadTasks();
    LoadOnePage();
    for (uint8_t id = TASK_TABLE_SIZE; id > 0; id--)
    {
        auto *task = get_task_struct(tasks[id - 1]);

        ASSERT_NE(task, nullptr);
        set_page_table(task->page_table);
        uint8_t previous_page_id = 0;
        for (uint8_t page_id = 1; page_id < PAGE_TABLE_SIZE; page_id++)
        {
            dprintf("Testing: task pid %d page %u\n", task->pid, page_id);

            uint16_t address = page_id * PAGE_SIZE + PAGE_SIZE / 2;
            int page_fault_res = page_fault(task->pid, address);
            ASSERT_EQ(page_fault_res, OK);
            dumpRamState();
            if (previous_page_id != 0)
            {
                uint8_t task_stored_data =
                    address_spaces[(task->pid * PAGE_TABLE_SIZE + previous_page_id) * PAGE_SIZE + PAGE_SIZE / 2];
                EXPECT_EQ(0xdd, task_stored_data);
                EXPECT_EQ(task->page_table[previous_page_id].m_bit, 0x0);
            }
            if (WRITE_PAGE(page_id))
            {
                int store_data_res = store_data(address, 0xdd);
                ASSERT_EQ(store_data_res, OK);
                previous_page_id = page_id;
            }
            else
            {
                previous_page_id = 0;
            }
        }
    }
}

TEST_F(Scale, MultipleModifiedPagesAreWrittenToAddressSpace)
{
    int ret = create_task(page_tables, PAGE_TABLE_SIZE, address_spaces);
    ASSERT_GE(ret, OK);
    auto *task = get_task_struct(ret);

    // load all pages -1 to ram
    for (uint8_t id = 0; id < PAGE_TABLE_SIZE - 1; id++)
    {
        int page_fault_res = page_fault(task->pid, PAGE_SIZE * id);
        ASSERT_EQ(page_fault_res, OK);
        EXPECT_EQ(task->page_table[id].p_bit, 0x1);
    }
    dumpRamState();

    // do some work in the task
    set_page_table(task->page_table);
    for (uint8_t id = 0; id < PAGE_TABLE_SIZE - 1; id++)
    {
        if (CODE_PAGE(id))
        {
            uint8_t data = 0;
            int fetch_instruction_res = fetch_instruction(id * PAGE_SIZE + 5, &data);
            EXPECT_EQ(fetch_instruction_res, OK);
            EXPECT_EQ(task->page_table[id].r_bit, 0x1);
        }
        else if (WRITE_PAGE(id))
        {
            int store_data_res = store_data(id * PAGE_SIZE + 5, 87);
            EXPECT_EQ(store_data_res, OK);
            EXPECT_EQ(task->page_table[id].m_bit, 0x1);
            EXPECT_EQ(task->page_table[id].r_bit, 0x1);
        }
        else
        {
            uint8_t data = 0;
            int load_data_res = load_data(id * PAGE_SIZE + 5, &data);
            EXPECT_EQ(load_data_res, OK);
            EXPECT_EQ(task->page_table[id].r_bit, 0x1);
        }
    }

    // load last page to ram
    int page_fault_res = page_fault(task->pid, PAGE_SIZE * (PAGE_TABLE_SIZE - 1));
    ASSERT_EQ(page_fault_res, OK);
    EXPECT_EQ(task->page_table[PAGE_TABLE_SIZE - 1].p_bit, 0x1);

    dumpRamState();
    for (uint8_t id = 0; id < PAGE_TABLE_SIZE; id++)
    {
        EXPECT_EQ(task->page_table[id].m_bit, 0x0);
        EXPECT_EQ(task->page_table[id].r_bit, 0x0);

        if (WRITE_PAGE(id) && id != PAGE_TABLE_SIZE - 1)
        {
            EXPECT_EQ(87, address_spaces[PAGE_SIZE * id + 5]);
        }
    }
}

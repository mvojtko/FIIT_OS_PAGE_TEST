#include "test_common.h"

#include <cstring>
#include <string>

bool operator==(const tTaskStruct &lhs, const tTaskStruct &rhs)
{
    return lhs.pid == rhs.pid &&
           lhs.ppid == rhs.ppid &&
           lhs.priority == rhs.priority &&
           lhs.signals == rhs.signals &&
           lhs.state == rhs.state &&
           lhs.uid == rhs.uid &&
           lhs.runtime == rhs.runtime &&
           lhs.start_time == rhs.start_time &&
           lhs.exit_code == rhs.exit_code;
}

void PrintTo(const tTaskStruct &entry, std::ostream *os)
{
    *os << "pid=" << std::to_string(entry.pid) <<
           ", ppid=" << std::to_string(entry.ppid) <<
           ", priority=" << std::to_string(entry.priority) <<
           ", signals=" << std::to_string(entry.signals) <<
           ", state=" << std::to_string(entry.state) <<
           ", uid=" << std::to_string(entry.uid) <<
           ", runtime=" << std::to_string(entry.runtime) <<
           ", start_time=" << std::to_string(entry.start_time) <<
           ", exit_code=" << std::to_string(entry.exit_code);
}

void OsTestFixture::SetUp()
{
    memset(tasks, 0xcd, sizeof(tasks));
    task_mgr.tasks = tasks + 1;
    task_mgr.size = kTaskMgrCapacity;
    ASSERT_EQ(init_taskMgr(&task_mgr), 0);
    CheckOverflow();
}

void OsTestFixture::TearDown()
{
    CheckOverflow();
    destroy_taskMgr();
    CheckOverflow();
}

void OsTestFixture::CheckOverflow() const
{
    tTaskStruct expected;
    memset(&expected, 0xcd, sizeof(tTaskStruct));
    EXPECT_EQ(memcmp(&tasks[0], &expected, sizeof(tTaskStruct)), 0)
        << "task storage guard was overwritten";
    EXPECT_EQ(memcmp(&tasks[kTaskStorageSlots - 1], &expected, sizeof(tTaskStruct)), 0)
        << "task storage guard was overwritten";
}

const tTaskStruct *OsTestFixture::GetTask(tPid pid) const
{
    for (size_t i = 0; i < kTaskStorageSlots; i++)
    {
        if (tasks[i].pid == pid)
        {
            return &tasks[i];
        }
    }
    return nullptr;
}

tPid OsTestFixture::CreateTask(uint8_t priority, tPid ppid, uint8_t uid)
{
    tPid pid = create_task(priority, ppid, uid);
    EXPECT_GT(pid, 0);
    return pid;
}

#pragma once

#include <ostream>

#include "debug.h"
#include "gtest/gtest.h"

extern "C" {
#include "task.h"
}

bool operator==(const tTaskStruct &lhs, const tTaskStruct &rhs);
void PrintTo(const tTaskStruct &entry, std::ostream *os);

// Shared task-manager setup for task and scheduler tests.
class OsTestFixture : public ::testing::Test
{
  protected:
    void SetUp() override;
    void TearDown() override;

    void CheckOverflow() const;
    const tTaskStruct *GetTask(tPid pid) const;
    tPid CreateTask(uint8_t priority = 0, tPid ppid = 0, uint8_t uid = 0);

    static constexpr size_t kTaskStorageSlots = 18;
    static constexpr uint8_t kTaskMgrCapacity = 16;

    tTaskStruct tasks[kTaskStorageSlots]{};
    tTaskMgr task_mgr{};
};

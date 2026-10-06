#include "test_common.h"

#include <cstring>
#include <string>

bool operator==(const tTaskStruct &lhs, const tTaskStruct &rhs)
{
    return memcmp(&lhs, &rhs, sizeof(tTaskStruct)) == 0;
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

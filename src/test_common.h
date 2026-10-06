#pragma once

#include <ostream>

extern "C" {
#include "task.h"
}

bool operator==(const tTaskStruct &lhs, const tTaskStruct &rhs);
void PrintTo(const tTaskStruct &entry, std::ostream *os);

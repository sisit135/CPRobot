// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace lifecycle {
int64_t now_us();
void run_tasks();
void advance_clock(int64_t dt);
std::vector<std::string> task_states();
} // namespace lifecycle

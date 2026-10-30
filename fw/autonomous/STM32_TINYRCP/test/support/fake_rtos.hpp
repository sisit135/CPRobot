#pragma once
#include "platform/rtos.hpp"
#include <functional>
namespace fake {
extern int64_t now;
void run(const char *task, const std::function<void()> &on_delay);
void finish();
} // namespace fake

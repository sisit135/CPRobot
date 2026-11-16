// SPDX-License-Identifier: GPL-3.0-only

#include "logging/log.hpp"
#include "support/fake_rtos.hpp"
#include <cassert>
#include <string>
#include <vector>

int main() {
  assert(robot_log::minimum_level() == robot_log::Level::TRACE);
  assert(robot_log::start());
  std::vector<std::string> records;
  robot_log::set_sink(
      robot_log::UART,
      [](const robot_log::Record &record, void *context) {
        static_cast<std::vector<std::string> *>(context)->push_back(
            record.message);
        return true;
      },
      &records);
  while (robot_log::write(robot_log::Level::TRACE, "vendor", "detail")) {
  }
  assert(robot_log::write(robot_log::Level::ERROR, "bup",
                          "health check failed; halted"));
  fake::run("boot_log", [] { fake::finish(); });
  assert(!records.empty());
  assert(records.back() == "health check failed; halted");
  assert(robot_log::dropped() >= 2);
}

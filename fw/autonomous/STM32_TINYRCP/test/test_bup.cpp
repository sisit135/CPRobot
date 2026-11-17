// SPDX-License-Identifier: GPL-3.0-only

#include "bup/bup_tasks.hpp"
#include "hal/boot_sensor_sample.hpp"
#include "logging/log.hpp"
#include "support/fake_rtos.hpp"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace {
std::string failure;
std::vector<std::string> steps, errors;
std::vector<std::string> table;
bool stopped = false;
unsigned captures = 0, prints = 0;
robot_log::Level threshold = robot_log::Level::TRACE;
bool services() {
  steps.push_back("services");
  return failure != "services";
}
} // namespace
bool hal_motor_init() {
  steps.push_back("motor");
  return failure != "motor";
}
void hal_motor_stop() { stopped = true; }
bool hal_sensors_init() {
  steps.push_back("sensors");
  return failure != "sensors";
}
namespace remote {
bool board_init() {
  steps.push_back("settings");
  return failure != "settings";
}
} // namespace remote
void boot_sensors::capture(Snapshot &) {
  assert(steps.back() == "sensors"); // Before application services own buses.
  ++captures;
}
void boot_sensors::print(const Snapshot &) {
  assert(bup::bup_task_get_status() != bup::BupStatus::RUNNING);
  if (threshold >= robot_log::Level::DEBUG)
    assert(table.back() == "└────────────┴────────────┘");
  ++prints;
}
namespace robot_log {
Level minimum_level() { return threshold; }
bool write(Level level, const char *source, const char *format, ...) {
  if (level == Level::ERROR || level == Level::DEBUG) {
    char text[128];
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof text, format, args);
    va_end(args);
    assert(std::string(text).size() < sizeof(Record{}.message));
    if (level == Level::ERROR)
      errors.push_back(std::string(source) + ": " + text);
    else
      table.push_back(text);
  }
  return true;
}
} // namespace robot_log
int main(int argc, char **argv) {
  assert(argc == 2 || argc == 3);
  failure = argv[1];
  if (argc == 3)
    threshold = robot_log::Level::INFO;
  assert(bup::bup_task_start(services));
  assert(steps.empty()); // HAL work happens in the task, not the creator.
  assert(!bup::bup_task_start(services));
  fake::run("bup_task", [] { assert(false); });
  const std::vector<std::string> all{"motor", "settings", "sensors",
                                     "services"};
  size_t expected = 4;
  if (failure == "motor")
    expected = 1;
  if (failure == "settings")
    expected = 2;
  if (failure == "sensors")
    expected = 3;
  assert(steps ==
         std::vector<std::string>(all.begin(), all.begin() + expected));
  const bool success = failure == "success";
  assert(bup::bup_task_get_status() ==
         (success ? bup::BupStatus::SUCCEEDED : bup::BupStatus::FAILED));
  assert(stopped == !success);
  assert(captures == (failure == "motor" || failure == "settings" ? 0u : 1u));
  assert(prints == 1);
  assert(errors.empty() == success);
  if (!success)
    assert(errors.front().find("halted") != std::string::npos);
  if (threshold < robot_log::Level::DEBUG) {
    assert(table.empty());
    return 0;
  }
  assert(table.size() == 10);
  assert(table.front() == "┌────────────┬────────────┐");
  assert(table.back() == "└────────────┴────────────┘");
  assert(table[8].find(success ? "ready" : "HALTED") != std::string::npos);
  if (failure == "motor") {
    assert(table[3].find("failed") != std::string::npos);
    assert(table[4].find("skipped") != std::string::npos);
    assert(table[5].find("skipped") != std::string::npos);
    assert(table[6].find("skipped") != std::string::npos);
  }
}

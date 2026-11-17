// SPDX-License-Identifier: GPL-3.0-only

#include "bup/bup_tasks.hpp"
#include "logging/log.hpp"
#include "motor_control/config.hpp"
#include "openloop_demo/openloop_demo.hpp"
#include "platform/rtos.hpp"
#include <Arduino.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

void setup();
namespace {
struct Task {
  TaskFunction_t function;
  std::string name;
  void *argument;
};
struct Leg {
  bool turning;
  int64_t begin, end = 0;
  double distance = 0, angle = 0;
};
struct Deleted {};
struct Finished {};
std::vector<Task> tasks;
std::vector<Leg> legs;
std::string scenario;
int64_t now = 1000000;
float duty_left = 0, duty_right = 0;
unsigned initializes = 0, writes = 0, key_reads = 0, starts = 0, errors = 0;
bool scheduled = false, bup_ran = false, stalled = false;

void run(const char *name) {
  for (const auto &task : tasks)
    if (task.name == name) {
      try {
        task.function(task.argument);
        assert(false);
      } catch (const Deleted &) {
        return;
      }
    }
  assert(false);
}
float wheel_speed(float duty, const motor::cfg::OpenLoopCalibration &c) {
  if (duty == 0)
    return 0;
  return duty > 0 ? (duty - c.forward_offset) / c.forward_duty_per_rad_s
                  : (duty + c.reverse_offset) / c.reverse_duty_per_rad_s;
}
} // namespace

bool hal_time_init() { return true; }
int64_t hal_time_us() { return now; }
bool hal_motor_init() {
  ++initializes;
  return scenario != "init_fault";
}
void hal_motor_stop() {
  duty_left = duty_right = 0;
  if (!legs.empty() && !legs.back().end)
    legs.back().end = now;
}
bool hal_motor_set_duty(float left, float right) {
  assert(key_reads == 10); // Two-sample bounce must not trigger motion.
  assert(std::fabs(left) <= motor::cfg::max_duty);
  assert(std::fabs(right) <= motor::cfg::max_duty);
  ++writes;
  if (scenario == "driver_fault" && writes == 12)
    return false;
  if (legs.empty() || legs.back().end)
    legs.push_back({left * right < 0, now});
  duty_left = left;
  duty_right = right;
  return true;
}
void pinMode(int pin, int mode) {
  assert(pin == AXI_LOC_KEY_PIN && mode == INPUT_PULLUP);
}
int digitalRead(int pin) {
  assert(pin == AXI_LOC_KEY_PIN);
  ++key_reads;
  assert(writes == 0);
  return key_reads <= 4 || key_reads == 7 ? HIGH : LOW;
}
namespace robot_log {
bool start() { return true; }
bool write(Level level, const char *, const char *format, ...) {
  char message[128];
  va_list args;
  va_start(args, format);
  std::vsnprintf(message, sizeof message, format, args);
  va_end(args);
  if (std::string(message).find("started: repeat") != std::string::npos)
    ++starts;
  if (level == Level::ERROR)
    ++errors;
  return true;
}
} // namespace robot_log
BaseType_t xTaskCreate(TaskFunction_t function, const char *name,
                       uint32_t words, void *argument, UBaseType_t,
                       TaskHandle_t *handle) {
  if (scenario == "bup_failed" && std::string(name) == "bup_task")
    return pdFALSE;
  assert(std::string(name) == "openloop_demo" ||
         std::string(name) == "bup_task");
  assert(words <= 1024);
  tasks.push_back({function, name, argument});
  *handle = reinterpret_cast<void *>(tasks.size());
  return pdPASS;
}
void vTaskStartScheduler() { scheduled = true; }
TickType_t xTaskGetTickCount() { return static_cast<TickType_t>(now / 1000); }
void vTaskDelete(TaskHandle_t) { throw Deleted{}; }
void vTaskDelayUntil(TickType_t *wake, TickType_t period) {
  *wake += period;
  const auto next = std::max(now, int64_t(*wake) * 1000);
  if (!legs.empty() && !legs.back().end) {
    const float left = wheel_speed(duty_left, motor::cfg::left_pwm);
    const float right = wheel_speed(duty_right, motor::cfg::right_pwm);
    const double dt = (next - now) * 1e-6;
    legs.back().distance +=
        (left + right) * .5 * motor::cfg::wheel_radius_m * dt;
    legs.back().angle += (right - left) * motor::cfg::wheel_radius_m /
                         motor::cfg::track_width_m * dt;
  }
  now = next;
  if (writes > 2 && !bup_ran && scenario != "bup_failed" &&
      scenario != "bup_pending") {
    const float left = duty_left, right = duty_right;
    run("bup_task"); // Running BUP must neither initialize nor stop DRV.
    bup_ran = true;
    assert(duty_left == left && duty_right == right && initializes == 1);
    assert(bup::bup_task_get_status() == bup::BupStatus::SUCCEEDED);
  }
  if (scenario == "timing_fault" && writes == 12 && !stalled) {
    now += 100000; // Outside the production converter's timing bounds.
    stalled = true;
  }
  if (legs.size() == 4 && legs.back().end)
    throw Finished{};
  assert(now < 20000000);
}

int main(int argc, char **argv) {
  assert(argc == 2);
  scenario = argv[1];
  setup(); // Link the actual demo entry without any sensor/store/nav symbols.
  assert(scheduled && tasks.front().name == "openloop_demo");
  assert(!openloop_demo::start());
  assert(bup::bup_task_get_status() == (scenario == "bup_failed"
                                            ? bup::BupStatus::FAILED
                                            : bup::BupStatus::RUNNING));
  bool completed = false;
  try {
    run("openloop_demo");
  } catch (const Finished &) {
    completed = true;
  }
  assert(initializes == 1 && duty_left == 0 && duty_right == 0);
  if (scenario == "init_fault") {
    assert(!completed && writes == 0 && starts == 0 && errors == 1);
  } else if (scenario == "driver_fault" || scenario == "timing_fault") {
    assert(!completed && starts == 1 && errors == 1);
  } else {
    assert(completed && starts == 1 && key_reads == 10);
    for (const auto &leg : legs) {
      if (leg.turning) {
        assert(leg.end - leg.begin == 1000000);
        assert(std::fabs(leg.angle + 1.57079632679) < .02);
        assert(std::fabs(leg.distance) < 1e-6);
      } else {
        assert(leg.distance >= .99999 && leg.distance < 1.003);
        assert(std::fabs(leg.angle) < 1e-6);
      }
    }
    assert(!legs[0].turning && legs[1].turning && !legs[2].turning &&
           legs[3].turning);
    if (scenario == "bup_pending")
      assert(bup::bup_task_get_status() == bup::BupStatus::RUNNING);
    if (scenario == "bup_failed")
      assert(bup::bup_task_get_status() == bup::BupStatus::FAILED);
  }
  std::printf("PASS demo %s: isolated startup, key latch and nominal motion\n",
              argv[1]);
}

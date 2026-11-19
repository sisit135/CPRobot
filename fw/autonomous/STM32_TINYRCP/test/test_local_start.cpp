// SPDX-License-Identifier: GPL-3.0-only
#include "Arduino.h"
#include "bup/bup_tasks.hpp"
#include "hal/boot_sensor_sample.hpp"
#include "localization/localization.hpp"
#include "logging/log.hpp"
#include "motor_control/motor_control.hpp"
#include "navigation/navigation.hpp"
#include "remote/board_store.hpp"
#include "semphr.h"
#include "support/fake_rtos.hpp"
#include <cassert>
#include <string>

void setup();
namespace {
lfs_t filesystem{};
remote::Store store(filesystem);
bool sensors_started = false, retry = false, locked = false;
unsigned navigation_calls = 0, sample = 0;
// Bounce and short presses precede two distinct, sustained presses.
const int levels[] = {HIGH, LOW, HIGH, LOW, LOW, HIGH, LOW, LOW,
                      LOW, LOW, LOW, HIGH, LOW, LOW, LOW, LOW};
}
bool hal_time_init() { return true; }
int64_t hal_time_us() { return fake::now; }
unsigned long micros() { return fake::now; }
bool hal_board_sensor_buses_init() { return true; }
bool hal_motor_init() { return true; }
void hal_motor_stop() { assert(false && "healthy startup must succeed"); }
bool hal_sensors_init() { return true; }
void hal_navigation_config(nav::ResolverConfig *) {}
void pinMode(int pin, int mode) {
  assert(pin == PB1 && mode == INPUT_PULLUP);
}
int digitalRead(int pin) {
  assert(pin == PB1);
  return levels[sample];
}
void digitalWrite(int, int) { assert(false); }

SemaphoreHandle_t xSemaphoreCreateMutex() { return &locked; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t handle, TickType_t) {
  assert(handle == &locked && !locked);
  locked = true;
  return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t handle) {
  assert(handle == &locked && locked);
  locked = false;
  return pdTRUE;
}
void vSemaphoreDelete(SemaphoreHandle_t) { assert(false); }
namespace remote {
bool board_init() { return true; }
Store *board_store() { return &store; }
}
namespace loc {
bool localization_sensors_start() {
  assert(!sensors_started);
  sensors_started = true;
  return true;
}
bool localization_get_bootstrap(nav::BootstrapFrame *) { return false; }
}
namespace nav {
bool navigation_start(const ResolverConfig &, NavigationEngine engine,
                      const FeatureConfig &, const RightHandConfig &) {
  assert(sensors_started && locked && engine == NavigationEngine::FEATURE);
  ++navigation_calls;
  return !retry || navigation_calls > 1;
}
void navigation_halt() { assert(false); }
}
namespace motor {
bool motor_stop() {
  assert(false && "no goal or stop command was issued");
  return false;
}
}
namespace boot_sensors {
void capture(Snapshot &) { assert(!sensors_started); }
void print(const Snapshot &) {
  assert(bup::bup_task_get_status() == bup::BupStatus::SUCCEEDED);
}
}
namespace robot_log {
bool start() { return true; }
Level minimum_level() { return Level::INFO; }
bool write(Level, const char *, const char *, ...) { return true; }
bool set_sink(Sink sink, SinkCallback, void *) { (void)sink; return false; }
size_t drain(size_t) { return 0; }
uint32_t dropped() { return 0; }
}
int main(int argc, char **argv) {
  assert(argc == 2);
  retry = std::string(argv[1]) == "retry";
  setup();
  assert(!sensors_started && navigation_calls == 0);
  fake::run("bup_task", [] { assert(false); });
  assert(sensors_started && navigation_calls == 0);
  assert(bup::bup_task_get_status() == bup::BupStatus::SUCCEEDED);
  fake::run("loc_key", [] {
    assert(!locked);
    if (sample < 8)
      assert(navigation_calls == 0); // Bounce never reaches three low samples.
    else if (sample < 14)
      assert(navigation_calls == 1); // A held key starts only once.
    else
      assert(navigation_calls == (retry ? 2u : 1u));
    if (++sample == sizeof(levels) / sizeof(levels[0]))
      fake::finish();
    fake::now += 20000;
  });
  assert(navigation_calls == (retry ? 2u : 1u));
}

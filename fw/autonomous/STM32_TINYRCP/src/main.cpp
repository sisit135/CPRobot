#include "bup/bup_tasks.hpp"
#include "localization/hal_sensors.hpp"
#include "localization/localization.hpp"
#include "motor_control/hal_motor.hpp"
#include "navigation/navigation.hpp"
#include "platform/rtos.hpp"
#include "robot_application.h"
#include <Arduino.h>
#ifdef AXI_REMOTE_CONFIG_ENABLE
#include "remote/board_store.hpp"
#endif
#ifdef AXI_TELEMETRY_SPI_ENABLE
#include "telemetry/adapter.hpp"
#endif
extern "C" bool robot_application_start() {
  // HAL is initialized by setup before any application task can run.
  nav::ResolverConfig config = nav::default_resolver_config();
  hal_navigation_config(&config);
  if (!bup::bup_task_start() || !loc::localization_sensors_start())
    return false;
#ifdef AXI_TELEMETRY_SPI_ENABLE
  return telemetry::start(config);
#else
  return nav::navigation_start(config);
#endif
}
void setup() {
  (void)hal_motor_init(); // HAL outputs are forced low; no PWM task exists yet.
#ifdef AXI_REMOTE_CONFIG_ENABLE
  (void)remote::board_init(); // Unavailable store keeps remote
                              // tuning/calibration gated.
#endif
  if (!hal_sensors_init() || !robot_application_start())
    return;
  vTaskStartScheduler();
}
void loop() { /* Reached only if startup/scheduler failed; no motor command. */
}

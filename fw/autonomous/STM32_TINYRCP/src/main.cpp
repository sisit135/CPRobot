// SPDX-License-Identifier: GPL-3.0-only

#include "bup/bup_tasks.hpp"
#include "localization/localization.hpp"
#include "logging/log.hpp"
#include "navigation/navigation.hpp"
#include "platform/hal_platform.hpp"
#include "platform/rtos.hpp"
#include "remote/board_store.hpp"
#include "remote/settings.hpp"
#include "robot_application.h"
#include <Arduino.h>
#ifdef AXI_TELEMETRY_SPI_ENABLE
#include "telemetry/adapter.hpp"
#endif
extern "C" bool robot_application_start() {
  // Called by bring-up only after HAL initialization succeeds.
  if (!remote::board_store())
    return false;
  nav::ResolverConfig config = nav::default_resolver_config();
  hal_navigation_config(&config);
  if (!loc::localization_sensors_start())
    return false;
#ifdef AXI_TELEMETRY_SPI_ENABLE
  return telemetry::start(config);
#else
  return nav::navigation_start(config, nav::NavigationEngine::FEATURE,
                               remote::feature_settings(),
                               remote::fallback_settings());
#endif
}
void setup() {
  if (!hal_time_init())
    return;
  if (!robot_log::start())
    return;
  if (!bup::bup_task_start(robot_application_start))
    return;
  vTaskStartScheduler();
}
void loop() { /* Reached only if startup/scheduler failed; no motor command. */
}

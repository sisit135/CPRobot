// SPDX-License-Identifier: GPL-3.0-only
#include "navigation/local_start.hpp"
#include "navigation/navigation.hpp"
#include "platform/rtos.hpp"
#include "remote/board_store.hpp"
#include "remote/settings.hpp"
#ifdef AXI_LOG_RTT_ENABLE
#include "hal/rtt_log.hpp"
#include "remote/at_cli.hpp"
#endif
#include <Arduino.h>
#include <semphr.h>
#ifndef AXI_LOC_KEY_PIN
#define AXI_LOC_KEY_PIN -1
#endif
namespace nav {
namespace {
ResolverConfig settings{};
SemaphoreHandle_t start_mutex = nullptr;
bool navigation_started = false;
void key_task(void *) {
  pinMode(AXI_LOC_KEY_PIN, INPUT_PULLUP);
  unsigned low_samples = 0;
  bool pressed = false;
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    if (digitalRead(AXI_LOC_KEY_PIN) == LOW) {
      if (low_samples < 3)
        ++low_samples;
      if (low_samples == 3 && !pressed) {
        pressed = true;
        xSemaphoreTake(start_mutex, portMAX_DELAY);
        if (!navigation_started && remote::board_store())
          navigation_started = navigation_start(settings, NavigationEngine::FEATURE,
              remote::feature_settings(), remote::fallback_settings());
        xSemaphoreGive(start_mutex);
      }
    } else {
      low_samples = 0;
      pressed = false;
    }
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(20));
  }
}
#ifdef AXI_LOG_RTT_ENABLE
remote::AtCli at_cli;
bool at_reply(const char *bytes, std::size_t count) {
  taskENTER_CRITICAL();
  const bool written = hal_rtt_try_write(bytes, count);
  taskEXIT_CRITICAL();
  return written;
}
void settings_task(void *) {
  for (;;) {
    xSemaphoreTake(start_mutex, portMAX_DELAY);
    at_cli.poll(remote::board_store(), !navigation_started, hal_rtt_read, at_reply);
    xSemaphoreGive(start_mutex);
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
#endif
}
bool local_start_service(const ResolverConfig &config) {
  if (start_mutex || AXI_LOC_KEY_PIN < 0)
    return false;
  settings = config;
  start_mutex = xSemaphoreCreateMutex();
  if (!start_mutex)
    return false;
  TaskHandle_t key_handle = nullptr;
  bool ok = xTaskCreate(key_task, "loc_key", 512, nullptr, 2, &key_handle) == pdPASS;
#ifdef AXI_LOG_RTT_ENABLE
  TaskHandle_t settings_handle = nullptr;
  if (ok)
    ok = xTaskCreate(settings_task, "local_settings", 2048, nullptr, 1,
                    &settings_handle) == pdPASS;
#endif
  if (!ok) {
    if (key_handle)
      vTaskDelete(key_handle);
#ifdef AXI_LOG_RTT_ENABLE
    if (settings_handle)
      vTaskDelete(settings_handle);
#endif
    vSemaphoreDelete(start_mutex);
    start_mutex = nullptr;
  }
  return ok;
}
}

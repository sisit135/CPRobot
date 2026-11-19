// SPDX-License-Identifier: GPL-3.0-only

#include "logging/log.hpp"
#ifdef AXI_LOG_RTT_ENABLE
#include "hal/rtt_log.hpp"
#endif

#include <Arduino.h>
#include <FreeRTOS.h>
#include <cstdio>
#include <cstring>
#include <task.h>

namespace robot_log {
namespace {
constexpr size_t kCapacity = 32;
Record ring[kCapacity];
volatile size_t head = 0, tail = 0, count = 0;
volatile uint32_t lost = 0;
volatile uint8_t threshold = static_cast<uint8_t>(Level::TRACE);
volatile uint8_t active_sinks = UART;
SinkCallback callbacks[3]{};
void *contexts[3]{};
bool started = false;

int index(Sink sink) {
  return sink == UART ? 0 : sink == RTT ? 2 : -1;
}
bool put(const Record &record) {
  taskENTER_CRITICAL();
  if (count == kCapacity) {
    ++lost;
    if (record.level > Level::WARN) {
      taskEXIT_CRITICAL();
      return false;
    }
    // A fatal bring-up report must survive a saturated vendor trace burst.
    tail = (tail + 1) % kCapacity;
    --count;
  }
  ring[head] = record;
  head = (head + 1) % kCapacity;
  ++count;
  taskEXIT_CRITICAL();
  return true;
}
bool get(Record *record) {
  taskENTER_CRITICAL();
  if (!count) {
    taskEXIT_CRITICAL();
    return false;
  }
  *record = ring[tail];
  tail = (tail + 1) % kCapacity;
  --count;
  taskEXIT_CRITICAL();
  return true;
}
[[maybe_unused]] size_t format_line(const Record &record, char *line,
                                  size_t capacity) {
  const int n = std::snprintf(line, capacity, "%lu %u %s: %s\n",
                              static_cast<unsigned long>(record.timestamp_ms),
                              static_cast<unsigned>(record.level),
                              record.source, record.message);
  return n > 0 && static_cast<size_t>(n) < capacity ? static_cast<size_t>(n) : 0;
}
void serial_sink(const Record &record) {
#ifdef AXI_LOG_SERIAL_ENABLE
  char line[128];
  const size_t size = format_line(record, line, sizeof line);
  if (size)
    Serial.write(reinterpret_cast<const uint8_t *>(line), size);
#else
  (void)record;
#endif
}
void rtt_sink(const Record &record) {
#ifdef AXI_LOG_RTT_ENABLE
  char line[128];
  const size_t size = format_line(record, line, sizeof line);
  // Serialize brief RTT writes with local settings replies.
  taskENTER_CRITICAL();
  (void)hal_log_rtt_write(line, size);
  taskEXIT_CRITICAL();
#else
  (void)record;
#endif
}
void early_log_task(void *) {
  for (;;) {
    (void)drain(kCapacity);
    vTaskDelay(pdMS_TO_TICKS(1) ? pdMS_TO_TICKS(1) : 1);
  }
}
} // namespace

bool start() {
  if (started)
    return true;
#ifdef AXI_LOG_SERIAL_ENABLE
  Serial.begin(115200);
#endif
  uint8_t sinks = UART;
#ifdef AXI_LOG_RTT_ENABLE
  sinks |= RTT;
#endif
  configure(Level::TRACE, sinks);
  TaskHandle_t task = nullptr;
  started =
      xTaskCreate(early_log_task, "boot_log", 512, nullptr, 2, &task) == pdPASS;
  return started;
}

void configure(Level minimum, uint8_t sinks) {
  threshold = static_cast<uint8_t>(minimum);
  active_sinks = sinks & (UART | RTT);
}
Level minimum_level() { return static_cast<Level>(threshold); }
uint8_t enabled_sinks() { return active_sinks; }
bool set_sink(Sink sink, SinkCallback callback, void *context) {
  const int i = index(sink);
  if (i < 0)
    return false;
  taskENTER_CRITICAL();
  callbacks[i] = callback;
  contexts[i] = context;
  taskEXIT_CRITICAL();
  return true;
}
bool write(Level level, const char *source, const char *format, ...) {
  va_list arguments;
  va_start(arguments, format);
  const bool result = writev(level, source, format, arguments);
  va_end(arguments);
  return result;
}
bool writev(Level level, const char *source, const char *format,
            va_list arguments) {
  if (!source || !format || static_cast<uint8_t>(level) > threshold)
    return false;
  Record record{};
  record.timestamp_ms =
      static_cast<uint32_t>(xTaskGetTickCount() * portTICK_PERIOD_MS);
  std::snprintf(record.source, sizeof record.source, "%s", source);
  const int n =
      std::vsnprintf(record.message, sizeof record.message, format, arguments);
  if (n < 0)
    return false;
  record.level = level;
  return put(record);
}
size_t drain(size_t maximum) {
  size_t delivered = 0;
  Record record{};
  while (delivered < maximum && get(&record)) {
    const uint8_t sinks = active_sinks;
    if (sinks & RTT) {
      if (callbacks[2])
        (void)callbacks[2](record, contexts[2]);
      else
        rtt_sink(record);
    }
    if (sinks & UART) {
      if (callbacks[0])
        (void)callbacks[0](record, contexts[0]);
      else
        serial_sink(record);
    }
    ++delivered;
  }
  return delivered;
}
uint32_t dropped() { return lost; }
} // namespace robot_log

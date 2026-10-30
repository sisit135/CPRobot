#include "logging/log.hpp"

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
volatile uint8_t threshold = static_cast<uint8_t>(Level::INFO);
volatile uint8_t active_sinks = TELEMETRY;
SinkCallback callbacks[2]{};
void *contexts[2]{};

int index(Sink sink) { return sink == SERIAL ? 0 : sink == TELEMETRY ? 1 : -1; }
bool put(const Record &record) {
  taskENTER_CRITICAL();
  if (count == kCapacity) {
    ++lost;
    taskEXIT_CRITICAL();
    return false;
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
void serial_sink(const Record &record) {
#ifdef AXI_LOG_SERIAL_ENABLE
  char line[128];
  const int n = std::snprintf(line, sizeof line, "%lu %u %s: %s\n",
                              static_cast<unsigned long>(record.timestamp_ms),
                              static_cast<unsigned>(record.level),
                              record.source, record.message);
  if (n > 0 && static_cast<size_t>(n) < sizeof line &&
      Serial.availableForWrite() >= n)
    Serial.write(reinterpret_cast<const uint8_t *>(line),
                 static_cast<size_t>(n));
#else
  (void)record;
#endif
}
} // namespace

void configure(Level minimum, uint8_t sinks) {
  threshold = static_cast<uint8_t>(minimum);
  active_sinks = sinks & (SERIAL | TELEMETRY);
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
    if (sinks & SERIAL)
      serial_sink(record);
    if ((sinks & TELEMETRY) && callbacks[1])
      (void)callbacks[1](record, contexts[1]);
    ++delivered;
  }
  return delivered;
}
uint32_t dropped() { return lost; }
} // namespace robot_log

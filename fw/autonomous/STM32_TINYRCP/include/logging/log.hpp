// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdint>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbuiltin-declaration-mismatch"
#endif
namespace robot_log {

enum class Level : uint8_t { ERROR = 0, WARN, INFO, DEBUG, TRACE };
// UART is the legacy serial sink bit; STM32 profiles route Serial via USB CDC.
enum Sink : uint8_t { UART = 1u << 0, TELEMETRY = 1u << 1, RTT = 1u << 2 };

/** A fixed-size record keeps logging deterministic and allocation-free. */
struct Record {
  uint32_t timestamp_ms;
  Level level;
  char source[16];
  char message[96];
};

using SinkCallback = bool (*)(const Record &record, void *context);

/** Configure TRACE and start early serial draining before bring-up.
 * Telemetry takes over deferred draining once its sink is attached. */
bool start();

/** Configure the minimum accepted level and enabled sink mask at runtime. */
void configure(Level minimum_level, uint8_t enabled_sinks);
Level minimum_level();
uint8_t enabled_sinks();

/** Attach a deferred sink. Callbacks run only from drain(), never from a
 * producer. */
bool set_sink(Sink sink, SinkCallback callback, void *context = nullptr);

/** Format and enqueue a record. Returns false when filtered, malformed, or
 * full for INFO/DEBUG/TRACE. WARN/ERROR replace the oldest entry on overflow.
 */
bool write(Level level, const char *source, const char *format, ...);
bool writev(Level level, const char *source, const char *format,
            va_list arguments);

/** Dequeue and deliver up to max_records from the early serial task or telemetry
 * vTask after its sink is attached. */
size_t drain(size_t max_records = 1);
uint32_t dropped();

} // namespace robot_log
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

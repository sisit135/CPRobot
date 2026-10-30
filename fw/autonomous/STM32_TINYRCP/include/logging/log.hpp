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
enum Sink : uint8_t { SERIAL = 1u << 0, TELEMETRY = 1u << 1 };

/** A fixed-size record keeps logging deterministic and allocation-free. */
struct Record {
  uint32_t timestamp_ms;
  Level level;
  char source[16];
  char message[96];
};

using SinkCallback = bool (*)(const Record &record, void *context);

/** Configure the minimum accepted level and enabled sink mask at runtime. */
void configure(Level minimum_level, uint8_t enabled_sinks);
Level minimum_level();
uint8_t enabled_sinks();

/** Attach a deferred sink. Callbacks run only from drain(), never from a
 * producer. */
bool set_sink(Sink sink, SinkCallback callback, void *context = nullptr);

/** Format and enqueue a record. Returns false when filtered, malformed, or
 * full. */
bool write(Level level, const char *source, const char *format, ...);
bool writev(Level level, const char *source, const char *format,
            va_list arguments);

/** Dequeue and deliver up to max_records. Intended for the telemetry vTask. */
size_t drain(size_t max_records = 1);
uint32_t dropped();

} // namespace robot_log
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// SPDX-License-Identifier: GPL-3.0-only
#include "remote/at_cli.hpp"
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef THS_FIRMWARE_VERSION
#define THS_FIRMWARE_VERSION "unversioned"
#endif

namespace remote {
namespace {
constexpr const char *table_format =
    "| %-5s | %-24s | %-48s | %-6s | %15s | %15s | %15s | %15s | %10s |\r\n";
void human_value(char (&out)[16], float value, uint8_t type) {
  if (!std::isfinite(value))
    std::snprintf(out, sizeof out, "n/a");
  else if (type == 2)
    std::snprintf(out, sizeof out, "%s", value != 0 ? "true" : "false");
  else
    std::snprintf(out, sizeof out, type == 1 ? "%.0f" : "%.9g",
                  static_cast<double>(value));
}
bool number(const char *text, uint32_t *out, const char **end) {
  if (*text < '0' || *text > '9')
    return false;
  errno = 0;
  char *last = nullptr;
  const auto value = std::strtoul(text, &last, 10);
  if (errno || value > UINT32_MAX)
    return false;
  *out = static_cast<uint32_t>(value);
  *end = last;
  return true;
}
} // namespace
void AtCli::table_border() {
  pending_ = 0;
  constexpr int widths[] = {5, 24, 48, 6, 15, 15, 15, 15, 10};
  for (const auto width : widths) {
    output_[pending_++] = '+';
    for (int i = 0; i < width + 2; ++i)
      output_[pending_++] = '-';
  }
  output_[pending_++] = '+';
  output_[pending_++] = '\r';
  output_[pending_++] = '\n';
}
void AtCli::status(Status result, uint32_t revision) {
  if (result == OK)
    pending_ = std::snprintf(output_, sizeof output_, "+REV:%lu\r\nOK\r\n",
                             static_cast<unsigned long>(revision));
  else
    pending_ = std::snprintf(output_, sizeof output_, "ERROR:%u\r\n",
                             static_cast<unsigned>(result));
}
void AtCli::execute(Store *store, bool idle) {
  const uint32_t revision = store ? store->snapshot().revision : 0;
  if (std::strcmp(input_, "AT+VER") == 0) {
    pending_ = std::snprintf(
        output_, sizeof output_,
        "+VER:THS,firmware=%.48s,at=1,settings=2,catalog=%08lx\r\nOK\r\n",
        THS_FIRMWARE_VERSION, static_cast<unsigned long>(kCatalogSignature));
    return;
  }
  if (std::strcmp(input_, "AT+NVLISTITEMS") == 0 ||
      std::strcmp(input_, "AT+NVLISTITEMSH") == 0) {
    list_index_ = 0;
    listing_ = true;
    human_list_ = std::strcmp(input_, "AT+NVLISTITEMSH") == 0;
    table_stage_ = 0;
    pending_ = std::snprintf(output_, sizeof output_,
                             human_list_ ? "+NVLISTITEMSH:%u\r\n"
                                         : "+NVLISTITEMS:%u\r\n",
                             static_cast<unsigned>(kFields));
    return;
  }
  const char *end = nullptr;
  uint32_t key = 0, expected = revision;
  bool set = false, reset = false;
  float value = 0;
  if (std::strncmp(input_, "AT+NVGETITEM=", 13) == 0) {
    if (!number(input_ + 13, &key, &end) || *end)
      goto syntax;
  } else if (std::strncmp(input_, "AT+NVSETITEM=", 13) == 0) {
    if (!number(input_ + 13, &key, &end) || *end != ',')
      goto syntax;
    const char *start = end + 1;
    char *last = nullptr;
    errno = 0;
    value = std::strtof(start, &last);
    if (last == start || errno || !std::isfinite(value))
      goto syntax;
    end = last;
    if (*end == ',' && !number(end + 1, &expected, &end))
      goto syntax;
    if (*end)
      goto syntax;
    set = true;
  } else if (std::strcmp(input_, "AT+FACTORYRESET") == 0) {
    reset = true;
  } else if (std::strncmp(input_, "AT+FACTORYRESET=", 16) == 0) {
    if (!number(input_ + 16, &expected, &end) || *end)
      goto syntax;
    reset = true;
  } else {
    goto syntax;
  }
  if ((set || reset) && !idle) {
    status(BUSY, revision);
    return;
  }
  if (!store || !store->ready()) {
    status(NOT_READY, revision);
    return;
  }
  if (set || reset) {
    const auto result = reset ? store->factory_reset(expected)
                              : store->write(key, value, expected);
    if (result == OK)
      apply(*store);
    status(result, store->snapshot().revision);
  } else if (store->index(key) < 0) {
    status(NOT_FOUND, revision);
  } else {
    pending_ = std::snprintf(
        output_, sizeof output_, "+NVGETITEM:%lu,%.9g,%lu\r\nOK\r\n",
        static_cast<unsigned long>(key), static_cast<double>(store->get(key)),
        static_cast<unsigned long>(revision));
  }
  return;
syntax:
  std::strcpy(output_, "ERROR:SYNTAX\r\n");
  pending_ = std::strlen(output_);
}
void AtCli::poll(Store *store, bool idle, Read read, Write write) {
  if (!read || !write)
    return;
  if (pending_) {
    if (pending_ >= sizeof output_ || !write(output_, pending_))
      return;
    pending_ = 0;
  }
  if (listing_) {
    if (human_list_ && table_stage_ < 3) {
      if (table_stage_ == 1)
        pending_ = std::snprintf(output_, sizeof output_, table_format, "Key",
                                 "Group", "Item", "Type", "Current", "Factory",
                                 "Min", "Max", "Revision");
      else
        table_border();
      ++table_stage_;
      return;
    }
    if (human_list_ && list_index_ == kFields && table_stage_ == 3) {
      table_border();
      ++table_stage_;
      return;
    }
    if (list_index_ == kFields) {
      listing_ = false;
      status(OK, store ? store->snapshot().revision : 0);
    } else {
      const auto &spec = kCatalog[list_index_++];
      if (human_list_) {
        char key[16], revision[16], current[16], initial[16], minimum[16],
            maximum[16];
        std::snprintf(key, sizeof key, "%lu",
                      static_cast<unsigned long>(spec.key));
        std::snprintf(
            revision, sizeof revision, "%lu",
            static_cast<unsigned long>(store ? store->snapshot().revision : 0));
        human_value(current,
                    store && store->ready() ? store->get(spec.key) : NAN,
                    spec.type);
        human_value(initial, spec.initial, spec.type);
        human_value(minimum, spec.min, spec.type);
        human_value(maximum, spec.max, spec.type);
        const char *type =
            spec.type == 2 ? "bool" : (spec.type == 1 ? "uint32" : "float");
        pending_ = std::snprintf(output_, sizeof output_, table_format, key,
                                 spec.group, spec.name, type, current, initial,
                                 minimum, maximum, revision);
        return;
      }
      pending_ = std::snprintf(
          output_, sizeof output_,
          "+NVITEM:%lu,%s,%u,%.9g,%.9g,%.9g,%.9g,%lu\r\n",
          static_cast<unsigned long>(spec.key), spec.name,
          static_cast<unsigned>(spec.type), static_cast<double>(spec.min),
          static_cast<double>(spec.max), static_cast<double>(spec.initial),
          static_cast<double>(store && store->ready() ? store->get(spec.key)
                                                      : NAN),
          static_cast<unsigned long>(store ? store->snapshot().revision : 0));
    }
    return;
  }
  for (unsigned budget = 0; budget < 32; ++budget) {
    const int byte = read();
    if (byte < 0)
      return;
    if (byte == '\r' || byte == '\n') {
      if (overflow_) {
        std::strcpy(output_, "ERROR:LINE_TOO_LONG\r\n");
        pending_ = std::strlen(output_);
      } else if (used_) {
        input_[used_] = 0;
        execute(store, idle);
      }
      used_ = 0;
      overflow_ = false;
      if (pending_)
        return;
    } else if (byte < 32 || byte > 126 || used_ + 1 >= sizeof input_) {
      overflow_ = true;
    } else if (!overflow_) {
      input_[used_++] = static_cast<char>(byte);
    }
  }
}
} // namespace remote

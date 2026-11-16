// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
namespace remote {
struct Spec {
  uint32_t key;
  const char *name, *group;
  uint32_t type;
  float min, max, initial;
  const char *source;
  uint32_t source_id;
};
} // namespace remote

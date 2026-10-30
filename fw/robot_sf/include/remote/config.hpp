// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "lfs.h"
#include "localization/color_tracker.hpp"
#include "remote/codec.h"
#include <array>
#include <cstddef>
#include <cstdint>
namespace remote {
enum Status : uint32_t {
  OK,
  INVALID,
  CONFLICT,
  BUSY,
  IO_ERROR,
  NOT_FOUND,
  NOT_READY
};
struct Spec {
  uint32_t key;
  const char *name, *group;
  uint32_t type;
  float min, max, initial;
  const char *source;
};
constexpr std::size_t kFields = 41;
const std::array<Spec, kFields> &catalog();
struct Snapshot {
  uint32_t flags = 1, revision = 1;
  std::array<float, kFields> values{};
};
class Store {
public:
  explicit Store(lfs_t &fs) : fs_(fs) {}
  Status init(); // ENOENT seeds defaults; damaged/version-mismatched records
                 // fail closed.
  Status write(uint32_t key, float value, uint32_t expected);
  Status calibrate(float gyro_bias, uint32_t expected);
  Status calibrate_color(uint32_t color_id, float red, float green, float blue,
                         uint32_t expected);
  const Snapshot &snapshot() const { return state_; }
  bool ready() const { return ready_; }
  int index(uint32_t key) const;
  float get(uint32_t key) const;

private:
  bool commit(const Snapshot &candidate);
  lfs_t &fs_;
  Snapshot state_{};
  bool ready_ = false;
};
void apply(const Store &store); // Call only while navigation has never started.
std::size_t color_references(const loc::ColorReference **out);
float gyro_zero();
// Request/reply service: caller serializes access with start/calibration mutex.
std::size_t handle(Store &store, const remote_request &request, bool idle,
                   const uint8_t *schema, std::size_t schema_size, uint8_t *out,
                   std::size_t capacity, bool (*calibrator)(float *) = nullptr);
} // namespace remote

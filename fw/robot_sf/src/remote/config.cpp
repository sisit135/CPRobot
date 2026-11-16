// SPDX-License-Identifier: GPL-3.0-only
#include "remote/config.hpp"
#include "remote/settings.hpp"
#include "remote/settings_bindings.hpp"
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
namespace remote {
namespace {
constexpr const char *path = "settings.bin", *temporary = "settings.tmp";
constexpr std::size_t record_size = 28 + 4 * kFields;
static_assert(record_size <= 256,
              "Flash record must fit a littlefs inline file");
nav::FeatureConfig features;
nav::RightHandConfig right_hand;
std::atomic<float> gyro_bias{0};
std::atomic<uint32_t> thresholds{0};
std::atomic<bool> published{false};
uint32_t crc(const uint8_t *bytes, std::size_t n) {
  uint32_t value = ~uint32_t{0};
  for (std::size_t i = 0; i < n; ++i) {
    value ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit)
      value = (value >> 1) ^ (0xedb88320u & (0u - (value & 1)));
  }
  return ~value;
}
void put(uint8_t *out, uint32_t value) {
  for (int i = 0; i < 4; ++i)
    out[i] = uint8_t(value >> (8 * i));
}
uint32_t get(const uint8_t *in) {
  return uint32_t(in[0]) | uint32_t(in[1]) << 8 | uint32_t(in[2]) << 16 |
         uint32_t(in[3]) << 24;
}
float value(const Snapshot &s, uint32_t key) {
  for (std::size_t i = 0; i < kFields; ++i)
    if (kCatalog[i].key == key)
      return s.values[i];
  return std::numeric_limits<float>::quiet_NaN();
}
bool valid(const Snapshot &s) {
  if (!s.revision || s.flags != 1)
    return false;
  for (std::size_t i = 0; i < kFields; ++i) {
    const auto &spec = kCatalog[i];
    const auto v = s.values[i];
    if (!std::isfinite(v) || v < spec.min || v > spec.max ||
        (spec.type != 0 && std::trunc(v) != v))
      return false;
  }
  const auto f = generated::feature(s.values);
  const auto r = generated::rhr(s.values);
  if (f.max_skew_us > f.max_age_us || r.wall_distance_m <= r.side_stop_m ||
      r.opening_m <= r.wall_distance_m || r.coast_us >= r.turn_timeout_us)
    return false;
  const auto black = value(s, generated::tape_black_max_key),
             white = value(s, generated::tape_white_min_key);
  if (black > 0 && white > 0 && black >= white)
    return false;
  for (uint32_t base : {100u, 110u, 120u, 130u})
    if (value(s, base) != 0 &&
        (value(s, base + 1) + value(s, base + 2) + value(s, base + 3) <= 0 ||
         (value(s, base + 4) == 0 &&
          (value(s, base + 5) > 4 || value(s, base + 6) > 4))))
      return false;
  return true;
}
void encode(const Snapshot &s, uint8_t *out) {
  put(out, 0x32534854u); // THS2, little endian.
  put(out + 4, 2);
  put(out + 8, kCatalogSignature);
  put(out + 12, s.revision);
  put(out + 16, s.flags);
  put(out + 20, kFields);
  for (std::size_t i = 0; i < kFields; ++i) {
    uint32_t bits;
    std::memcpy(&bits, &s.values[i], 4);
    put(out + 24 + 4 * i, bits);
  }
  put(out + record_size - 4, crc(out, record_size - 4));
}
bool decode(const uint8_t *in, Snapshot *s) {
  if (get(in) != 0x32534854u || get(in + 4) != 2 ||
      get(in + 8) != kCatalogSignature || get(in + 20) != kFields ||
      get(in + record_size - 4) != crc(in, record_size - 4))
    return false;
  s->revision = get(in + 12);
  s->flags = get(in + 16);
  for (std::size_t i = 0; i < kFields; ++i) {
    const uint32_t bits = get(in + 24 + 4 * i);
    std::memcpy(&s->values[i], &bits, 4);
  }
  return valid(*s);
}
} // namespace
const std::array<Spec, kFields> &catalog() { return kCatalog; }
int Store::index(uint32_t key) const {
  for (std::size_t i = 0; i < kFields; ++i)
    if (kCatalog[i].key == key)
      return int(i);
  return -1;
}
float Store::get(uint32_t key) const {
  return ready_ ? value(state_, key) : std::numeric_limits<float>::quiet_NaN();
}
Status Store::init() {
  ready_ = false;
  uint8_t cache[256], bytes[record_size];
  lfs_file_t file{};
  lfs_file_config cfg{};
  cfg.buffer = cache;
  int rc = lfs_file_opencfg(&fs_, &file, path, LFS_O_RDONLY, &cfg);
  if (rc == LFS_ERR_NOENT) {
    state_ = {};
    for (std::size_t i = 0; i < kFields; ++i)
      state_.values[i] = kCatalog[i].initial;
    ready_ = valid(state_) && commit(state_);
    return ready_ ? OK : IO_ERROR;
  }
  if (rc < 0)
    return IO_ERROR;
  const auto size = lfs_file_size(&fs_, &file);
  const auto read = lfs_file_read(&fs_, &file, bytes, sizeof bytes);
  rc = lfs_file_close(&fs_, &file);
  Snapshot candidate;
  if (rc < 0 || size != lfs_soff_t(record_size) ||
      read != lfs_ssize_t(record_size) || !decode(bytes, &candidate))
    return IO_ERROR;
  state_ = candidate;
  ready_ = true;
  return OK;
}
bool Store::commit(const Snapshot &candidate) {
  uint8_t bytes[record_size], cache[256];
  encode(candidate, bytes);
  lfs_file_t file{};
  lfs_file_config cfg{};
  cfg.buffer = cache;
  if (lfs_file_opencfg(&fs_, &file, temporary,
                       LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC, &cfg) < 0)
    return false;
  const bool written = lfs_file_write(&fs_, &file, bytes, sizeof bytes) ==
                       lfs_ssize_t(sizeof bytes);
  const bool synced = written && lfs_file_sync(&fs_, &file) == 0;
  const bool closed = lfs_file_close(&fs_, &file) == 0;
  return synced && closed && lfs_rename(&fs_, temporary, path) == 0;
}
Status Store::write(uint32_t key, float v, uint32_t expected) {
  if (!ready_)
    return NOT_READY;
  const int i = index(key);
  if (i < 0)
    return NOT_FOUND;
  if (expected != state_.revision || expected == UINT32_MAX)
    return CONFLICT;
  auto next = state_;
  next.values[i] = v;
  if (!valid(next))
    return INVALID;
  if (state_.values[i] == v)
    return OK;
  ++next.revision;
  if (!commit(next)) {
    ready_ = false; // A failed commit is unconfirmed; remount before reuse.
    return IO_ERROR;
  }
  state_ = next;
  return OK;
}
Status Store::calibrate(float bias, uint32_t expected) {
  return write(generated::gyro_zero_key, bias, expected);
}
Status Store::calibrate_color(uint32_t slot, float red, float green, float blue,
                              uint32_t expected) {
  if (!ready_)
    return NOT_READY;
  if (expected != state_.revision || expected == UINT32_MAX)
    return CONFLICT;
  const float sum = red + green + blue;
  if (slot >= 4 || !std::isfinite(sum) || sum <= 0 || red < 0 || green < 0 ||
      blue < 0)
    return INVALID;
  auto next = state_;
  const uint32_t base = 100 + 10 * slot;
  next.values[index(base + 1)] = red / sum;
  next.values[index(base + 2)] = green / sum;
  next.values[index(base + 3)] = blue / sum;
  if (!valid(next))
    return INVALID;
  if (next.values == state_.values)
    return OK;
  ++next.revision;
  if (!commit(next)) {
    ready_ = false;
    return IO_ERROR;
  }
  state_ = next;
  return OK;
}
void apply(const Store &store) {
  if (!store.ready())
    return;
  features = generated::feature(store.snapshot().values);
  right_hand = generated::rhr(store.snapshot().values);
  gyro_bias.store(store.get(generated::gyro_zero_key),
                  std::memory_order_release);
  const uint32_t black = uint32_t(store.get(generated::tape_black_max_key)),
                 white = uint32_t(store.get(generated::tape_white_min_key));
  thresholds.store(black | (white << 16), std::memory_order_release);
  published.store(true, std::memory_order_release);
}
nav::FeatureConfig feature_settings() { return features; }
nav::RightHandConfig fallback_settings() { return right_hand; }
float gyro_zero() { return gyro_bias.load(std::memory_order_acquire); }
bool tape_thresholds(uint16_t *black, uint16_t *white) {
  if (!black || !white || !published.load(std::memory_order_acquire))
    return false;
  const auto packed = thresholds.load(std::memory_order_acquire);
  *black = uint16_t(packed);
  *white = uint16_t(packed >> 16);
  return true;
}
std::size_t color_references(const loc::ColorReference **out) {
  if (out)
    *out = nullptr;
  return 0; // Stored legacy references never restore color navigation.
}
} // namespace remote

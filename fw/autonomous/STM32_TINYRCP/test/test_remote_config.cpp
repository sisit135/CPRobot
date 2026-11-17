// SPDX-License-Identifier: GPL-3.0-only
#include "proto_buffer.hpp"
#include "remote/config.hpp"
#include "remote/flash_layout.hpp"
#include "remote/schema_blob.hpp"
#include "remote/settings.hpp"
#include "remote_config.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace {
namespace layout = remote::flash_layout;
using Reply = axi::remote::v1::RemoteReply<48, 24, 64, 160>;
struct Image {
  std::vector<uint8_t> bytes =
      std::vector<uint8_t>(layout::blocks * layout::block_size, 255);
  int cut = -1, operations = 0;
  bool failed = false, partial = false;
  bool operation() {
    if (failed)
      return false;
    if (cut == operations++) {
      failed = true;
      return false;
    }
    return true;
  }
};
struct Filesystem {
  Image &image;
  lfs_t fs{};
  lfs_config cfg{};
  uint8_t read_cache[layout::cache_size]{}, prog_cache[layout::cache_size]{},
      lookahead[8]{};
  Filesystem(Image &i, bool format = false) : image(i) {
    cfg.context = &image;
    cfg.read_size = cfg.prog_size = 4;
    cfg.block_size = layout::block_size;
    cfg.block_count = layout::blocks;
    cfg.cache_size = layout::cache_size;
    cfg.lookahead_size = sizeof lookahead;
    cfg.block_cycles = 100;
    cfg.metadata_max = layout::metadata_max;
    cfg.inline_max = cfg.file_max = layout::file_max;
    cfg.name_max = layout::name_max;
    cfg.read_buffer = read_cache;
    cfg.prog_buffer = prog_cache;
    cfg.lookahead_buffer = lookahead;
    cfg.read = [](const lfs_config *c, lfs_block_t b, lfs_off_t off, void *out,
                  lfs_size_t n) {
      auto &image = *static_cast<Image *>(c->context);
      assert(b < layout::blocks && off + n <= layout::block_size);
      if (image.failed)
        return int(LFS_ERR_IO);
      std::memcpy(out, image.bytes.data() + b * layout::block_size + off, n);
      return 0;
    };
    cfg.prog = [](const lfs_config *c, lfs_block_t b, lfs_off_t off,
                  const void *in, lfs_size_t n) {
      auto &image = *static_cast<Image *>(c->context);
      assert(b < layout::blocks && off + n <= layout::block_size &&
             !(off % 4) && !(n % 4));
      const bool already_failed = image.failed;
      const bool ok = image.operation();
      const auto *bytes = static_cast<const uint8_t *>(in);
      const auto count =
          ok ? n : ((!already_failed && image.partial) ? (n / 2 / 4) * 4 : 0);
      for (lfs_size_t index = 0; index < count; ++index) {
        auto &dest = image.bytes[b * layout::block_size + off + index];
        assert((dest & bytes[index]) == bytes[index]);
        dest &= bytes[index];
      }
      return ok ? 0 : int(LFS_ERR_IO);
    };
    cfg.erase = [](const lfs_config *c, lfs_block_t b) {
      auto &image = *static_cast<Image *>(c->context);
      assert(b < layout::blocks);
      if (!image.operation())
        return int(LFS_ERR_IO);
      std::fill_n(image.bytes.begin() + b * layout::block_size,
                  layout::block_size, uint8_t(255));
      return 0;
    };
    cfg.sync = [](const lfs_config *c) {
      return static_cast<Image *>(c->context)->operation() ? 0
                                                           : int(LFS_ERR_IO);
    };
    if (format)
      assert(lfs_format(&fs, &cfg) == 0);
    assert(lfs_mount(&fs, &cfg) == 0);
  }
  ~Filesystem() { assert(lfs_unmount(&fs) == 0); }
};
Reply call(remote::Store &store, uint32_t op, uint32_t key = 0, float value = 0,
           uint32_t revision = 0, uint32_t offset = 0, bool idle = true,
           bool (*calibrator)(float *) = nullptr) {
  uint8_t bytes[238];
  remote_request request{UINT32_MAX, op, key, revision, offset, value};
  const auto size = remote::handle(store, request, idle, remote::schema_blob,
                                   sizeof remote::schema_blob, bytes,
                                   sizeof bytes, calibrator);
  assert(size > 0 && size <= sizeof bytes);
  Reply reply;
  assert(tool_proto::decode(reply, bytes, size));
  assert(reply.request_id() == request.id);
  return reply;
}
uint32_t code(const Reply &reply) { return uint32_t(reply.status()); }
void defaults_and_rpc() {
  Image image;
  Filesystem fs(image, true);
  remote::Store store(fs.fs);
  assert(store.init() == remote::OK && store.ready());
  const auto initial = store.snapshot();
  assert(initial.revision == 1 && initial.flags == 1);
  remote::apply(store);
  assert(remote::feature_settings().contradiction_limit == 3);
  assert(remote::fallback_settings().speed_m_s ==
         nav::RightHandConfig{}.speed_m_s);
  uint16_t black = 1, white = 1;
  assert(remote::tape_thresholds(&black, &white) && black == 0 && white == 0);
  image.operations = 0;
  for (std::size_t i = 0; i < remote::kFields; ++i) {
    const auto &spec = remote::catalog()[i];
    auto reply = call(store, 2, 0, 0, 0, i);
    assert(code(reply) == remote::OK && reply.total() == remote::kFields &&
           reply.offset() == i);
    const auto &field = reply.field();
    assert(field.key() == spec.key &&
           std::strcmp(field.get_name().get_const(), spec.name) == 0);
    assert(uint32_t(field.type()) == spec.type && field.minimum() == spec.min &&
           field.maximum() == spec.max &&
           field.default_value() == spec.initial &&
           field.source_id() == spec.source_id);
    assert(call(store, 3, spec.key).value() == spec.initial);
    assert(store.write(spec.key, spec.initial, store.snapshot().revision) ==
           remote::OK);
  }
  assert(image.operations ==
         0); // Every unchanged write is genuinely read-only.
  assert(code(call(store, 2, 0, 0, 0, remote::kFields)) == remote::NOT_FOUND);
  assert(code(call(store, 3, 999)) == remote::NOT_FOUND);
  assert(code(call(store, 0)) == remote::INVALID);
  assert(code(call(store, 4, 206, 5, 1, 0, false)) == remote::BUSY);
  assert(code(call(store, 4, 206, 5, 0)) == remote::CONFLICT);
  assert(code(call(store, 4, 206, 1, 1)) == remote::INVALID);
  assert(code(call(store, 4, 206, 3.5f, 1)) == remote::INVALID);
  assert(code(call(store, 4, 206, std::numeric_limits<float>::quiet_NaN(),
                   1)) == remote::INVALID);
  assert(image.operations == 0);
  auto written = call(store, 4, 206, 5, 1);
  assert(code(written) == remote::OK && written.revision() == 2 &&
         written.value() == 5);
  assert(remote::feature_settings().contradiction_limit == 5);
  assert(code(call(store, 4, 206, 5, 1)) ==
         remote::CONFLICT); // No replay with stale revision.
  auto revision = store.snapshot().revision;
  assert(store.write(207, 50000, revision) ==
         remote::INVALID); // max_age < max_skew.
  assert(store.write(224, .05f, revision) ==
         remote::INVALID); // Side stop exceeds wall distance.
  assert(code(call(store, 4, 240, 150, revision)) == remote::OK);
  revision = store.snapshot().revision;
  assert(code(call(store, 4, 241, 100, revision)) == remote::INVALID);
  assert(code(call(store, 4, 241, 800, revision)) == remote::OK);
  assert(remote::tape_thresholds(&black, &white) && black == 150 &&
         white == 800);
  revision = store.snapshot().revision;
  auto calibration = [](float *value) {
    *value = .02f;
    return true;
  };
  assert(code(call(store, 5, 0, 0, revision - 1, 0, true, calibration)) ==
         remote::CONFLICT);
  assert(code(call(store, 5, 0, 0, revision, 0, false, calibration)) ==
         remote::BUSY);
  assert(code(call(store, 5, 0, 0, revision, 0, true, calibration)) ==
         remote::OK);
  assert(std::fabs(remote::gyro_zero() - .02f) < 1e-7f);
  revision = store.snapshot().revision;
  assert(store.calibrate_color(0, 80, 15, 5, revision) == remote::OK);
  assert(std::fabs(store.get(101) - .8f) < 1e-6f);
  assert(store.write(105, 5, store.snapshot().revision) ==
         remote::OK); // Disabled slot may be edited.
  assert(store.write(100, 1, store.snapshot().revision) ==
         remote::INVALID); // Cell index 5 is invalid.
  const loc::ColorReference *references = nullptr;
  assert(remote::color_references(&references) ==
         0); // No reintroduction of mapped floor colors.
  std::vector<uint8_t> descriptor;
  while (descriptor.size() < sizeof remote::schema_blob) {
    auto reply = call(store, 1, 0, 0, 0, descriptor.size());
    assert(code(reply) == remote::OK &&
           reply.total() == sizeof remote::schema_blob &&
           reply.offset() == descriptor.size());
    const auto &chunk = reply.get_schema_chunk();
    assert(chunk.get_length() > 0 && chunk.get_length() <= 160);
    descriptor.insert(descriptor.end(), chunk.get_const(),
                      chunk.get_const() + chunk.get_length());
  }
  assert(std::equal(descriptor.begin(), descriptor.end(), remote::schema_blob));
  assert(code(call(store, 1, 0, 0, 0, descriptor.size())) == remote::NOT_FOUND);
  for (uint32_t source = 1; source <= 5; ++source) {
    auto reply = call(store, 6, source);
    assert(code(reply) == remote::OK && reply.total() > 0 &&
           reply.get_schema_chunk().get_length() > 0);
  }
  assert(code(call(store, 6, 99)) == remote::NOT_FOUND);
  const auto saved = store.snapshot();
  image.operations = 0;
  remote::Store rebooted(fs.fs);
  assert(rebooted.init() == remote::OK &&
         rebooted.snapshot().values == saved.values &&
         rebooted.snapshot().revision == saved.revision &&
         image.operations == 0);
  // Truncated protobuf is rejected before the service sees any mutation.
  axi::remote::v1::RemoteRequest request;
  request.set_request_id(7);
  request.set_operation(axi::remote::v1::Operation::WRITE);
  request.set_key(206);
  request.set_value(4);
  request.set_expected_revision(saved.revision);
  uint8_t bytes[238];
  const auto size = tool_proto::encode(request, bytes, sizeof bytes);
  remote_request decoded{};
  assert(remote_decode(bytes, size, &decoded) && decoded.id == 7 &&
         decoded.key == 206);
  assert(!remote_decode(bytes, size - 1, &decoded));
  assert(!remote_decode(nullptr, 0, &decoded));
}
void power_cuts() {
  Image original;
  remote::Snapshot old;
  {
    Filesystem fs(original, true);
    remote::Store store(fs.fs);
    assert(store.init() == remote::OK);
    old = store.snapshot();
  }
  auto next_image = original;
  remote::Snapshot next;
  int operations = 0;
  {
    Filesystem fs(next_image);
    remote::Store store(fs.fs);
    assert(store.init() == remote::OK);
    next_image.operations = 0;
    assert(store.calibrate_color(0, 80, 15, 5, old.revision) == remote::OK);
    next = store.snapshot();
    operations = next_image.operations;
  }
  assert(operations > 0);
  for (bool partial : {false, true})
    for (int cut = 0; cut < operations; ++cut) {
      auto image = original;
      {
        Filesystem fs(image);
        remote::Store store(fs.fs);
        assert(store.init() == remote::OK);
        image.operations = 0;
        image.cut = cut;
        image.partial = partial;
        assert(store.calibrate_color(0, 80, 15, 5, old.revision) ==
               remote::IO_ERROR);
        assert(!store.ready());
      }
      image.failed = false;
      image.cut = -1;
      Filesystem recovered(image);
      remote::Store store(recovered.fs);
      assert(store.init() == remote::OK);
      const auto &actual = store.snapshot();
      assert(
          (actual.revision == old.revision && actual.values == old.values) ||
          (actual.revision == next.revision && actual.values == next.values));
    }
  std::printf("Power-cut recovery: %d operation boundaries, with "
              "atomic/partial program cuts\n",
              operations);
}
void damage_and_wear() {
  Image image;
  Filesystem fs(image, true);
  remote::Store store(fs.fs);
  assert(store.init() == remote::OK);
  // Repeated commits compact the same two physical erase blocks; no data block
  // allocation or out-of-reservation write is permitted by the callbacks.
  for (int i = 0; i < 1200; ++i)
    assert(store.write(206, i % 2 ? 3 : 4, store.snapshot().revision) ==
           remote::OK);
  uint8_t cache[layout::cache_size], invalid[28 + 4 * remote::kFields]{};
  lfs_file_config cfg{};
  cfg.buffer = cache;
  lfs_file_t file{};
  assert(lfs_file_opencfg(&fs.fs, &file, "settings.bin",
                          LFS_O_WRONLY | LFS_O_TRUNC, &cfg) == 0);
  assert(lfs_file_write(&fs.fs, &file, invalid, sizeof invalid) ==
         sizeof invalid);
  assert(lfs_file_close(&fs.fs, &file) == 0);
  image.operations = 0;
  remote::Store damaged(fs.fs);
  assert(damaged.init() == remote::IO_ERROR && !damaged.ready() &&
         image.operations == 0);
}
} // namespace
int main() {
  defaults_and_rpc();
  power_cuts();
  damage_and_wear();
  std::puts("PASS required settings: generated catalog, bounded RPC, "
            "persistence, recovery, runtime bindings");
}

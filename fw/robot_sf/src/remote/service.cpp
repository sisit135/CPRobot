// SPDX-License-Identifier: GPL-3.0-only
#include "remote/protocol/proto_buffer.hpp"
#include "remote/config.hpp"
#include "remote/source_blob.hpp"
#include "remote/protocol/remote_config.h"
#include <algorithm>
namespace remote {
std::size_t handle(Store &store, const remote_request &request, bool idle,
                   const uint8_t *schema, std::size_t schema_size, uint8_t *out,
                   std::size_t capacity, bool (*calibrator)(float *)) {
  using Reply = axi::remote::v1::RemoteReply<48, 24, 64, 160>;
  if (!request.id || !out)
    return 0;
  capacity = std::min<std::size_t>(238, capacity);
  Reply reply;
  reply.set_request_id(request.id);
  Status status = OK;
  const uint8_t *bytes = nullptr;
  std::size_t size = 0;
  switch (request.op) {
  case 1:
    bytes = schema;
    size = schema_size;
    break;
  case 6:
    if (!source_page(request.key, &bytes, &size))
      status = NOT_FOUND;
    break;
  case 2:
    if (request.offset >= kFields)
      status = NOT_FOUND;
    else {
      const auto &spec = kCatalog[request.offset];
      auto &field = reply.mutable_field();
      field.set_key(spec.key);
      field.mutable_name().set(spec.name);
      field.mutable_group().set(spec.group);
      field.set_type(static_cast<axi::remote::v1::ValueType>(spec.type));
      field.set_minimum(spec.min);
      field.set_maximum(spec.max);
      field.mutable_source().set(spec.source);
      field.set_default_value(spec.initial);
      field.set_source_id(spec.source_id);
      reply.set_total(kFields);
      reply.set_offset(request.offset);
    }
    break;
  case 3:
    if (!store.ready())
      status = NOT_READY;
    else if (store.index(request.key) < 0)
      status = NOT_FOUND;
    else
      reply.set_value(store.get(request.key));
    break;
  case 4:
    status =
        idle ? store.write(request.key, request.value, request.revision) : BUSY;
    if (status == OK) {
      apply(store);
      reply.set_value(store.get(request.key));
    }
    break;
  case 5: {
    float bias = 0;
    if (!idle)
      status = BUSY;
    else if (!store.ready())
      status = NOT_READY;
    else if (request.revision != store.snapshot().revision)
      status = CONFLICT;
    else if (!calibrator || !calibrator(&bias))
      status = NOT_READY;
    else {
      status = store.calibrate(bias, request.revision);
      if (status == OK) {
        apply(store);
        reply.set_value(bias);
      }
    }
    break;
  }
  default:
    status = INVALID;
    break;
  }
  if (status == OK && (request.op == 1 || request.op == 6)) {
    if (!bytes || size > 65536 || request.offset >= size)
      status = NOT_FOUND;
    else {
      const auto count = std::min<std::size_t>(160, size - request.offset);
      reply.mutable_schema_chunk().set(bytes + request.offset, count);
      reply.set_total(size);
      reply.set_offset(request.offset);
    }
  }
  reply.set_status(static_cast<axi::remote::v1::Status>(status));
  reply.set_revision(store.snapshot().revision);
  reply.set_provisioned(store.ready() && (store.snapshot().flags & 1));
  return tool_proto::encode(reply, out, capacity);
}
} // namespace remote

// SPDX-License-Identifier: GPL-3.0-only
#include "remote/codec.h"
#include "proto_buffer.hpp"
#include "remote_config.h"
extern "C" int remote_decode(const uint8_t *data, size_t size,
                             remote_request *out) {
  if (!out)
    return 0;
  axi::remote::v1::RemoteRequest request;
  if (!tool_proto::decode(request, data, size))
    return 0;
  *out = {request.get_request_id(), uint32_t(request.get_operation()),
          request.get_key(),        request.get_expected_revision(),
          request.get_offset(),     request.get_value()};
  return out->id != 0;
}
extern "C" size_t remote_status(uint8_t *out, size_t capacity, uint32_t id,
                                uint32_t status) {
  axi::remote::v1::RemoteReply<1, 1, 1, 1> reply;
  reply.set_request_id(id);
  reply.set_status(static_cast<axi::remote::v1::Status>(status));
  return tool_proto::encode(reply, out, capacity);
}

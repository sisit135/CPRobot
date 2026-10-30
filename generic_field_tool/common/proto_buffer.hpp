// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "ReadBufferFixedSize.h"
#include "WriteBufferFixedSize.h"
#include <cstring>
namespace tool_proto {
template <class Message>
bool decode(Message &message, const uint8_t *data, size_t size) {
  if (!data || size > 238)
    return false;
  EmbeddedProto::ReadBufferFixedSize<238> buffer;
  memcpy(buffer.get_data(), data, size);
  buffer.set_bytes_written(size);
  return message.deserialize(buffer) == EmbeddedProto::Error::NO_ERRORS &&
         buffer.get_size() == 0;
}
template <class Message>
size_t encode(const Message &message, uint8_t *data, size_t capacity) {
  if (!data)
    return 0;
  EmbeddedProto::WriteBufferFixedSize<240> buffer;
  if (message.serialize(buffer) != EmbeddedProto::Error::NO_ERRORS ||
      buffer.get_size() > capacity)
    return 0;
  memcpy(data, buffer.get_data(), buffer.get_size());
  return buffer.get_size();
}
} // namespace tool_proto

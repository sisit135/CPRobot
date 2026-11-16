// SPDX-License-Identifier: GPL-3.0-only
#include "protocol_codec.h"
#include "remote/codec.h"
#include "remote_config.h"
#include "telemetry.h"
#include "protocol.h"
#include "proto_buffer.hpp"
using namespace axi::remote::v1;
int tool_control_decode(const uint8_t *data,size_t size,uint32_t *id,uint32_t *operation)
{
    axi::telemetry::v1::Control message;
    if (!tool_proto::decode(message,data,size)) return 0;
    *id=message.get_request_id();*operation=message.get_operation();return *id!=0;
}
int tool_color_decode(const uint8_t *data,size_t size,uint32_t *id,uint32_t *color)
{
    CalibrateColorRequest message;
    if (!tool_proto::decode(message,data,size)) return 0;
    *id=message.get_request_id();*color=message.get_color_id_mapped();return *id!=0;
}
int tool_envelope_valid(const uint8_t *data,size_t size)
{
    generic::field::tool::v1::Envelope<238,238> envelope;
    return tool_proto::decode(envelope,data,size) && envelope.get_request_id()!=0 && envelope.get_type().get_length()>0;
}
uint32_t tool_reply_id(const uint8_t *data,size_t size)
{
    RemoteReply<64,64,96,238> reply;
    return tool_proto::decode(reply,data,size)?reply.get_request_id():0;
}
size_t tool_ack_encode(uint8_t *out,size_t capacity,uint32_t id,uint32_t operation,uint32_t status)
{
    axi::telemetry::v1::ControlAck message;
    message.set_request_id(id);message.set_operation(operation);message.set_status(status);
    return tool_proto::encode(message,out,capacity);
}
size_t tool_link_encode(uint8_t *out,size_t capacity,uint32_t id,const char *address,const char *pin,uint32_t port,const char *identity)
{
    axi::telemetry::v1::LinkInfo<64,64,32> message;
    if (strlen(address)>64 || strlen(pin)>64 || strlen(identity)>32) return 0;
    message.set_request_id(id);message.set_port(port);
    message.mutable_ipv6_linklocal().set(address);message.mutable_certificate_sha256().set(pin);message.mutable_device_id().set(identity);
    return tool_proto::encode(message,out,capacity);
}

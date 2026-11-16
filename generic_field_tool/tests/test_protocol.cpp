// SPDX-License-Identifier: GPL-3.0-only
#include "protocol_codec.h"
#include "remote/codec.h"
#include "protocol.h"
#include "telemetry.h"
#include "proto_buffer.hpp"
#include "wire.h"
#include <cassert>
#include <cstdio>
int main()
{
    uint32_t id=0,value=99;
    // Proto3 omits the default color slot zero. It must remain valid.
    const uint8_t color[]={8,7};
    assert(tool_color_decode(color,sizeof color,&id,&value));assert(id==7&&value==0);
    const uint8_t truncated[]={8,0x80};
    assert(!tool_color_decode(truncated,sizeof truncated,&id,&value));
    uint8_t bytes[240];
    size_t size=tool_ack_encode(bytes,sizeof bytes,17,2,0);
    axi::telemetry::v1::ControlAck ack;
    assert(tool_proto::decode(ack,bytes,size));assert(ack.get_request_id()==17&&ack.get_operation()==2&&ack.get_status()==0);
    generic::field::tool::v1::Envelope<238,238> envelope;
    envelope.set_request_id(21);envelope.mutable_type().set("test.EchoCommand");
    const uint8_t payload[]={10,2,'h','i'};envelope.mutable_payload().set(payload,sizeof payload);
    size=tool_proto::encode(envelope,bytes,sizeof bytes);assert(tool_envelope_valid(bytes,size));
    for(uint8_t kind: {9,10,11}){
        uint8_t cell[AXI_SPI_SIZE];assert(axi_spi_typed_pack(kind,bytes,size,cell));
        const uint8_t *decoded=nullptr;size_t length=0;uint8_t type=0;
        assert(axi_spi_typed_unpack(cell,&type,&decoded,&length));assert(type==kind&&length==size);
        cell[5]^=1;assert(!axi_spi_typed_unpack(cell,&type,&decoded,&length));
    }
    // Test both ends of the complete-envelope bound, including type overhead.
    uint8_t padded[240]={0};
    envelope.mutable_payload().set(padded,215);
    size=tool_proto::encode(envelope,bytes,sizeof bytes);assert(size==238&&tool_envelope_valid(bytes,size));
    envelope.mutable_payload().set(padded,216);
    size=tool_proto::encode(envelope,bytes,sizeof bytes);assert(size==239&&!tool_envelope_valid(bytes,size));
    uint8_t cell[AXI_SPI_SIZE];assert(!axi_spi_typed_pack(9,bytes,size,cell));
    size=remote_status(bytes,sizeof bytes,99,3);assert(tool_reply_id(bytes,size)==99);
    puts("EmbeddedProto adapters, envelope bounds, typed SPI and CRC checks passed");
}

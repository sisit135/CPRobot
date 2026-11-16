// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote.proto;

import com.google.protobuf.ByteString;

import java.io.*;
import java.util.HashMap;

/**
 * Descriptor-backed envelope; protobuf owns all wire encoding and parsing.
 */
public record Envelope(int requestId, String type, byte[] payload) {
    private static final String TYPE = "generic.field.tool.v1.Envelope";
    private static final DynamicProto SCHEMA = loadSchema();

    private static DynamicProto loadSchema() {
        try (var input = Envelope.class.getResourceAsStream("/field_tool.desc")) {
            if (input == null) throw new IOException("Missing field_tool.desc");
            var data = new ByteArrayOutputStream();
            byte[] buffer = new byte[4096];
            int count;
            while ((count = input.read(buffer)) != -1) data.write(buffer, 0, count);
            return new DynamicProto(data.toByteArray());
        } catch (Exception error) {
            throw new ExceptionInInitializerError(error);
        }
    }

    public byte[] encode() throws IOException {
        var fields = new HashMap<String, Object>();
        fields.put("request_id", requestId);
        fields.put("type", type);
        fields.put("payload", ByteString.copyFrom(payload));
        byte[] result = SCHEMA.build(TYPE, fields).toByteArray();
        if (result.length > 238) throw new IOException("Envelope exceeds 238-byte SPI payload");
        return result;
    }

    public static Envelope decode(byte[] bytes) throws IOException {
        if (bytes.length > 238) throw new IOException("Envelope exceeds SPI limit");
        var message = SCHEMA.parse(TYPE, bytes);
        String type = (String) DynamicProto.field(message, "type");
        if (type.trim().isEmpty()) throw new IOException("Missing protobuf type");
        return new Envelope(DynamicProto.number(message, "request_id"), type, ((ByteString) DynamicProto.field(message, "payload")).toByteArray());
    }
}

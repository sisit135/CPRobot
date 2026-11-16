// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote.adl;

import android.content.Context;

import com.google.protobuf.DynamicMessage;

import dev.ra.remote.proto.DynamicProto;
import dev.ra.remote.proto.Envelope;
import dev.ra.remote.transport.TransportProvider;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * File-loaded provider; this module is deliberately absent from the app's runtime dependencies.
 */
public final class AdlTransportProvider implements TransportProvider {
    @Override
    public int apiVersion() {
        return API_VERSION;
    }

    @Override
    public String id() {
        return "adl.cyberpi.raw.v1";
    }

    @Override
    public String label() {
        return "ADL / ADR · CyberPi Bluetooth";
    }

    @Override
    public byte[] descriptorSet() {
        return AdlCodec.descriptor();
    }

    @Override
    public Connection create(Context context, Map<String, String> options) throws Exception {
        String address = options.getOrDefault("ble", "");
        if (!android.bluetooth.BluetoothAdapter.checkBluetoothAddress(address))
            throw new IllegalArgumentException("Set the CyberPi BLE address first");
        return new AdlConnection(new CyberPiBle(context, address));
    }

    static final class AdlConnection implements Connection {
        private final CyberPiBle ble;
        private final DynamicProto schema;
        private final ConcurrentHashMap<Integer, Integer> pending = new ConcurrentHashMap<>();
        private final StringBuilder buffer = new StringBuilder();
        private byte[] chunk = new byte[0];
        private int cursor;

        AdlConnection(CyberPiBle ble) throws Exception {
            this.ble = ble;
            this.schema = new DynamicProto(AdlCodec.descriptor());
        }

        @Override
        public void open() throws Exception {
            ble.open();
            // A read-only capability handshake. No arm, live-mode/reset, replay or motion heartbeat.
            ble.write((AdlCodec.seal("1|0|6") + "\n").getBytes(StandardCharsets.US_ASCII));
            int[] hello = AdlCodec.reply(line(5000));
            if (hello[0] != 0 || hello[1] != 6 || hello[2] != 0)
                throw new IOException("CyberPi field-tool firmware handshake rejected");
        }

        private String line(long timeout) throws Exception {
            long deadline = timeout == 0 ? Long.MAX_VALUE : System.nanoTime() + timeout * 1000000;
            while (true) {
                if (cursor >= chunk.length) {
                    long remain = timeout == 0 ? 0 : Math.max(1, (deadline - System.nanoTime()) / 1000000);
                    if (timeout != 0 && System.nanoTime() >= deadline)
                        throw new IOException("CyberPi field-tool handshake timed out; upload fw/manual/main.py");
                    chunk = ble.read(remain);
                    cursor = 0;
                }
                int b = chunk[cursor++] & 255;
                if (b == '\n') {
                    String result = buffer.toString();
                    buffer.setLength(0);
                    if (!result.isEmpty()) return result;
                } else {
                    if (b < 32 || b > 126 || buffer.length() >= 160)
                        throw new IOException("Malformed CyberPi stream");
                    buffer.append((char) b);
                }
            }
        }

        @Override
        public Packet read() throws Exception {
            while (true) {
                int[] response = AdlCodec.reply(line(0));
                Integer expected = pending.remove(response[0]);
                if (expected == null) continue;
                if (expected != response[1])
                    throw new IOException("Unexpected CyberPi response opcode");
                String type = AdlCodec.NAMES[response[1]] + "Response";
                String detail = switch (response[2]) {
                    case 0 ->
                            response[1] == 6 ? "Status" : "Admitted to input mailbox; actuator completion is not confirmed";
                    case 1 -> "Rejected: neutral arm required";
                    case 2 -> "Rejected: unsupported operation";
                    default -> "Rejected";/**/
                };
                Map<String, Object> fields = new java.util.HashMap<>();
                fields.put("accepted", response[2] == 0);
                fields.put("armed", response[3] != 0);
                fields.put("detail", detail);
                DynamicMessage message = schema.build(type, fields);
                return new Packet(10, new Envelope(response[0], type, message.toByteArray()).encode());
            }
        }

        @Override
        public void send(int kind, byte[] payload) throws Exception {
            if (kind != 9) throw new IOException("CyberPi provider supports schema requests only");
            Envelope envelope = Envelope.decode(payload);
            int op = AdlCodec.opcode(envelope.type());
            String wire = AdlCodec.encode(envelope.requestId(), envelope.type(), schema.parse(envelope.type(), envelope.payload()));
            // RobotSession admits one correlated operation at a time. Bound old timed-out IDs.
            if (pending.size() > 32) pending.clear();
            pending.put(envelope.requestId(), op);
            try {
                ble.write((wire + "\n").getBytes(StandardCharsets.US_ASCII));
            } catch (Exception e) {
                pending.remove(envelope.requestId());
                throw e;
            }
        }

        @Override
        public void close() {
            ble.close();
            pending.clear();
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote.proto;

import java.io.*;
import java.util.Arrays;

public final class Frames {
    public record Frame(int kind, byte[] payload) {
    }

    public static byte[] encode(int kind, byte[] p) {
        if (p.length < 1 || p.length > 240 || kind < 1 || kind > 255)
            throw new IllegalArgumentException("Frame bounds");
        byte[] b = new byte[p.length + 3];
        int n = p.length + 1;
        b[0] = (byte) n;
        b[1] = (byte) (n >> 8);
        b[2] = (byte) kind;
        System.arraycopy(p, 0, b, 3, p.length);
        return b;
    }

    public static Frame decode(byte[] b) throws IOException {
        if (b.length < 4) throw new IOException("Short frame");
        int n = (b[0] & 255) | ((b[1] & 255) << 8);
        if (n < 2 || n > 241 || n != b.length - 2) throw new IOException("Invalid frame length");
        return new Frame(b[2] & 255, Arrays.copyOfRange(b, 3, b.length));
    }

    public static Frame read(InputStream in) throws IOException {
        int a = in.read(), b = in.read();
        if (a < 0 || b < 0) throw new EOFException();
        int n = a | (b << 8);
        if (n < 2 || n > 241) throw new IOException("Invalid stream length");
        byte[] bytes = new byte[n + 2];
        bytes[0] = (byte) a;
        bytes[1] = (byte) b;
        int i = 2;
        while (i < bytes.length) {
            int take = in.read(bytes, i, bytes.length - i);
            if (take < 0) throw new EOFException();
            i += take;
        }
        return decode(bytes);
    }

    /**
     * NUS b7,id,index,count fragments. Reject partial/mixed/oversized frames; never retry.
     */
    public static final class Chunks {
        private int id = -1, next = 0, count = 0;
        private final ByteArrayOutputStream bytes = new ByteArrayOutputStream();

        public synchronized Frame accept(byte[] b) throws IOException {
            if (b.length < 5 || (b[0] & 255) != 0xb7) {
                reset();
                throw new IOException("Invalid NUS fragment");
            }
            int ident = b[1] & 255, index = b[2] & 255, total = b[3] & 255;
            if (index == 0) {
                reset();
                id = ident;
                count = total;
            }
            if (total == 0 || ident != id || index != next || total != count || bytes.size() + b.length - 4 > 243) {
                reset();
                throw new IOException("Incomplete NUS frame");
            }
            bytes.write(b, 4, b.length - 4);
            next++;
            if (next == count) {
                byte[] result = bytes.toByteArray();
                reset();
                return decode(result);
            }
            return null;
        }

        public synchronized void reset() {
            id = -1;
            next = 0;
            count = 0;
            bytes.reset();
        }
    }
}

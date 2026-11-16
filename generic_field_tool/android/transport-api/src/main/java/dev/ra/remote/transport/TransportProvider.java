// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote.transport;

import android.content.Context;
import java.io.Closeable;
import java.util.Map;

/** Host-owned ABI. Plugins must compileOnly this module and must have a public no-arg constructor. */
public interface TransportProvider {
    int API_VERSION = 1;
    int apiVersion();
    String id();
    String label();
    /** Binary FileDescriptorSet: synthetic request/response schemas are supported. */
    byte[] descriptorSet();
    Connection create(Context applicationContext, Map<String, String> options) throws Exception;

    /** Blocking I/O; the host calls on IO threads. close must unblock open/read/send. */
    interface Connection extends Closeable {
        void open() throws Exception;
        Packet read() throws Exception;
        /** Generic commands use kind 9 with the host's Envelope encoding. No implicit retry. */
        void send(int kind, byte[] payload) throws Exception;
        @Override void close();
    }

    record Packet(int kind, byte[] payload) {
    }
}

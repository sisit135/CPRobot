// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote.proto;

import com.google.protobuf.*;
import com.google.protobuf.util.JsonFormat;

import java.util.*;

/**
 * Schemas are data, never executable code. Bootstrap field numbers remain stable.
 */
public final class DynamicProto {
    public static final int MAX_DESCRIPTOR = 65536;
    private final Map<String, Descriptors.Descriptor> messages = new HashMap<>();

    public DynamicProto(byte[] bytes) throws Exception {
        if (bytes.length == 0 || bytes.length > MAX_DESCRIPTOR)
            throw new IllegalArgumentException("Schema size outside limit");
        DescriptorProtos.FileDescriptorSet set = DescriptorProtos.FileDescriptorSet.parseFrom(bytes);
        if (set.getFileCount() > 128) throw new IllegalArgumentException("Too many schema files");
        Map<String, DescriptorProtos.FileDescriptorProto> pending = new LinkedHashMap<>();
        for (var f : set.getFileList())
            if (pending.put(f.getName(), f) != null)
                throw new IllegalArgumentException("Duplicate schema file");
        Map<String, Descriptors.FileDescriptor> built = new HashMap<>();
        while (!pending.isEmpty()) {
            boolean progress = false;
            for (var it = pending.entrySet().iterator(); it.hasNext(); ) {
                var entry = it.next();
                var f = entry.getValue();
                if (!built.keySet().containsAll(f.getDependencyList())) continue;
                var dependencies = f.getDependencyList().stream().map(built::get).toArray(Descriptors.FileDescriptor[]::new);
                var fd = Descriptors.FileDescriptor.buildFrom(f, dependencies);
                built.put(f.getName(), fd);
                for (var d : fd.getMessageTypes()) register(d, 0);
                it.remove();
                progress = true;
            }
            if (!progress)
                throw new IllegalArgumentException("Schema has missing or cyclic imports");
        }
    }

    private void register(Descriptors.Descriptor d, int depth) {
        if (depth > 32 || messages.size() > 1024)
            throw new IllegalArgumentException("Schema complexity outside limit");
        if (messages.put(d.getFullName(), d) != null)
            throw new IllegalArgumentException("Duplicate protobuf type");
        for (var n : d.getNestedTypes()) register(n, depth + 1);
    }

    public Set<String> names() {
        return Collections.unmodifiableSet(messages.keySet());
    }

    public Descriptors.Descriptor descriptor(String name) {
        var d = messages.get(name);
        if (d == null) throw new IllegalArgumentException("Unknown message " + name);
        return d;
    }

    public DynamicMessage parse(String name, byte[] bytes) throws InvalidProtocolBufferException {
        return DynamicMessage.parseFrom(descriptor(name), bytes);
    }

    public byte[] json(String name, String json) throws Exception {
        var b = DynamicMessage.newBuilder(descriptor(name));
        JsonFormat.parser().merge(json, b);
        return b.build().toByteArray();
    }

    public String json(DynamicMessage m) throws Exception {
        return JsonFormat.printer().includingDefaultValueFields().omittingInsignificantWhitespace().print(m);
    }

    public DynamicMessage build(String name, Map<String, Object> fields) {
        var b = DynamicMessage.newBuilder(descriptor(name));
        for (var e : fields.entrySet()) {
            var f = b.getDescriptorForType().findFieldByName(e.getKey());
            if (f == null) throw new IllegalArgumentException("Unknown field " + e.getKey());
            Object v = e.getValue();
            if (f.getJavaType() == Descriptors.FieldDescriptor.JavaType.ENUM)
                v = f.getEnumType().findValueByNumber(((Number) v).intValue());
            if (v == null) throw new IllegalArgumentException("Unknown enum value");
            b.setField(f, v);
        }
        return b.build();
    }

    public static Object field(DynamicMessage m, String name) {
        var f = m.getDescriptorForType().findFieldByName(name);
        if (f == null) throw new IllegalArgumentException(name);
        return m.getField(f);
    }

    public static int number(DynamicMessage m, String name) {
        Object v = field(m, name);
        return v instanceof Descriptors.EnumValueDescriptor ? ((Descriptors.EnumValueDescriptor) v).getNumber() : ((Number) v).intValue();
    }

    public static float decimal(DynamicMessage m, String name) {
        return ((Number) field(m, name)).floatValue();
    }

    public static boolean flag(DynamicMessage m, String name) {
        return (Boolean) field(m, name);
    }

    public void requireBootstrapCompatible(DynamicProto bootstrap) {
        for (String name : Arrays.asList("axi.remote.v1.RemoteRequest", "axi.remote.v1.RemoteReply", "axi.remote.v1.FieldSpec")) {
            var old = bootstrap.descriptor(name);
            var current = descriptor(name);
            for (var f : old.getFields()) {
                var n = current.findFieldByNumber(f.getNumber());
                if (n == null || !n.getName().equals(f.getName()) || n.getType() != f.getType() || n.isRepeated() != f.isRepeated())
                    throw new IllegalArgumentException("Bootstrap incompatible: " + name);
            }
        }
    }
}

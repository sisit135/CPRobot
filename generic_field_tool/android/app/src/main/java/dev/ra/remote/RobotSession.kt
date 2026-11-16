// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import com.google.protobuf.ByteString
import com.google.protobuf.DynamicMessage
import dev.ra.remote.proto.DynamicProto
import dev.ra.remote.proto.Frames
import dev.ra.remote.proto.Envelope
import kotlinx.coroutines.*
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.json.JSONArray
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.security.MessageDigest
import java.util.concurrent.atomic.AtomicInteger

const val REQUEST = "axi.remote.v1.RemoteRequest"
const val REPLY = "axi.remote.v1.RemoteReply"
const val CONTROL = "axi.telemetry.v1.Control"
const val ACK = "axi.telemetry.v1.ControlAck"
const val COLOR_CALIBRATION = "axi.remote.v1.CalibrateColorRequest"

data class ConfigField(
    val key: Int,
    val name: String,
    val group: String,
    val type: Int,
    val minimum: Float,
    val maximum: Float,
    val source: String,
    val value: Float = 0f,
    val factoryDefault: Float? = null,
    val sourceId: Int = 0
)

data class RobotConfig(val fields: List<ConfigField>, val revision: Int, val provisioned: Boolean)
class RobotSession(
    val link: Link, private val bootstrap: DynamicProto, private val scope: CoroutineScope,
    private val onMessage: (Int, String) -> Unit, private val onLost: (Throwable) -> Unit
) {
    @Volatile
    var schema = bootstrap; private set
    var schemaHash = ""; private set
    private val ids = AtomicInteger(java.security.SecureRandom().nextInt(0x3fffffff) + 1)
    private val lock = Mutex()
    private var reader: Job? = null
    @Volatile
    private var pending: Triple<Int, Int, CompletableDeferred<DynamicMessage>>? = null
    @Volatile
    private var closed = false
    @Volatile
    private var genericPending: Pair<Int, CompletableDeferred<Envelope>>? = null
    @Volatile
    var projectSchema: DynamicProto? = null

    suspend fun start(loadRemoteSchema: Boolean = true, progress: (String) -> Unit) {
        link.open()
        reader = scope.launch(Dispatchers.IO) {
            try {
                while (isActive) {
                    val frame = try {
                        link.read()
                    } catch (e: LinkSwitched) {
                        pending?.third?.completeExceptionally(e); genericPending?.second?.completeExceptionally(
                            e
                        ); onMessage(255, e.message.orEmpty()); continue
                    }
                    if (frame.kind() == 10 || frame.kind() == 11) {
                        val envelope = Envelope.decode(frame.payload())
                        if (frame.kind() == 10) {
                            val p =
                                genericPending; if (p != null && p.first == envelope.requestId()) p.second.complete(
                                envelope
                            )
                        } else projectSchema?.let { registry ->
                            onMessage(
                                11,
                                "${envelope.type()}\n${
                                    registry.json(
                                        registry.parse(
                                            envelope.type(),
                                            envelope.payload()
                                        )
                                    )
                                }"
                            )
                        }
                        continue
                    }
                    val type = when (frame.kind()) {
                        1 -> "axi.telemetry.v1.EkfLog"; 3 -> ACK; 5 -> REPLY; 7 -> "axi.telemetry.v1.LogRecord"; else -> null
                    }
                    if (type != null) {
                        val message = schema.parse(type, frame.payload())
                        if (frame.kind() == 3 || frame.kind() == 5) {
                            val p =
                                pending; if (p != null && p.first == frame.kind() && p.second == DynamicProto.number(
                                    message,
                                    "request_id"
                                )
                            ) p.third.complete(message)
                        }
                        // Config pages/descriptor chunks are represented by loading progress, not thousands of log rows.
                        if (frame.kind() != 5) onMessage(frame.kind(), schema.json(message))
                    } else onMessage(
                        frame.kind(),
                        "Unknown kind ${frame.kind()} · ${frame.payload().size} bytes"
                    )
                }
            } catch (e: Throwable) {
                if (!closed) {
                    pending?.third?.completeExceptionally(e); genericPending?.second?.completeExceptionally(
                        e
                    ); onLost(e)
                }
            }
        }
        if (!loadRemoteSchema) return
        val bytes = ByteArrayOutputStream()
        var total = -1
        do {
            val r = rpc(1, offset = bytes.size()); ok(r)
            val chunk = (DynamicProto.field(r, "schema_chunk") as ByteString).toByteArray()
            val size = DynamicProto.number(r, "total")
            require(size in 1..DynamicProto.MAX_DESCRIPTOR && (total < 0 || total == size)) { "Invalid schema size" }
            require(
                DynamicProto.number(
                    r,
                    "offset"
                ) == bytes.size() && chunk.isNotEmpty() && bytes.size() + chunk.size <= size
            ) { "Invalid schema chunk" }
            total =
                size; bytes.write(chunk); progress("Loading protobuf schema · ${bytes.size()} / $total bytes")
        } while (bytes.size() < total)
        val loaded = DynamicProto(bytes.toByteArray()); loaded.requireBootstrapCompatible(bootstrap)
        schema = loaded; schemaHash =
            MessageDigest.getInstance("SHA-256").digest(bytes.toByteArray())
                .joinToString("") { "%02x".format(it) }
    }

    suspend fun sendCommand(type: String, json: String): String = lock.withLock {
        val registry = projectSchema ?: schema
        val suffix = if (type.substringAfterLast('.').endsWith("Request")) "Request" else "Command"
        require(type.substringAfterLast('.').endsWith(suffix)) { "Select a *Request or *Command message" }
        registry.descriptor(type.removeSuffix(suffix) + "Response")
        val id = ids.updateAndGet { if (it == Int.MAX_VALUE) 1 else it + 1 }
        val done = CompletableDeferred<Envelope>(); genericPending = id to done
        try {
            val encoded = Envelope(id, type, registry.json(type, json)).encode()
            link.send(9, encoded)
            val response = withTimeout(8000) { done.await() }
            require(response.type() == type.removeSuffix(suffix) + "Response") { "Unexpected response type ${response.type()}" }
            "${response.type()} · ${
                registry.json(
                    registry.parse(
                        response.type(),
                        response.payload()
                    )
                )
            }"
        } finally {
            genericPending = null
        }
    }

    private suspend fun exchange(
        kind: Int,
        replyKind: Int,
        type: String,
        fields: Map<String, Any>
    ): DynamicMessage = lock.withLock {
        val id = ids.updateAndGet { if (it == Int.MAX_VALUE) 1 else it + 1 }
        val done = CompletableDeferred<DynamicMessage>(); pending = Triple(replyKind, id, done)
        try {
            link.send(
                kind,
                schema.build(type, fields + mapOf("request_id" to id)).toByteArray()
            ); withTimeout(8000) { done.await() }
        } catch (e: TimeoutCancellationException) {
            throw IllegalStateException(
                "No ACK/NACK received; operation unconfirmed. Fetch before sending again.",
                e
            )
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            throw IllegalStateException(
                "Connection interrupted; operation unconfirmed. Fetch before sending again. ${e.message}",
                e
            )
        } finally {
            pending = null
        }
    }

    suspend fun rpc(op: Int, key: Int = 0, value: Float = 0f, revision: Int = 0, offset: Int = 0) =
        exchange(
            4,
            5,
            REQUEST,
            mapOf(
                "operation" to op,
                "key" to key,
                "value" to value,
                "expected_revision" to revision,
                "offset" to offset
            )
        )

    private fun ok(r: DynamicMessage) {
        val status = DynamicProto.number(r, "status"); check(status == 0) {
            when (status) {
                2 -> "Config changed on the robot. Fetch again before writing."; 3 -> "Robot busy. Tune before starting localization."; 4 -> "Flash I/O failed; write is unconfirmed. Fetch to inspect."; 6 -> "Config or stationary sensor samples unavailable."; else -> "Robot rejected request (status $status)"
            }
        }
    }

    suspend fun control(op: Int) {
        val r = exchange(2, 3, CONTROL, mapOf("operation" to op)); ok(r)
    }

    suspend fun fetch(progress: (Int, Int) -> Unit): RobotConfig {
        val fields = mutableListOf<ConfigField>()
        var total = 1
        var revision: Int? = null
        var provisioned = false
        for (i in 0 until 256) {
            if (i >= total) break
            val r = rpc(2, offset = i); ok(r)
            val rev = DynamicProto.number(r, "revision")
            require(revision == null || revision == rev) { "Config changed during fetch" }; revision =
                rev
            total = DynamicProto.number(r, "total"); require(total in 1..256)
            require(DynamicProto.number(r, "offset") == i)
            provisioned = DynamicProto.flag(r, "provisioned")
            val f = DynamicProto.field(r, "field") as DynamicMessage
            val key = DynamicProto.number(f, "key"); require(fields.none { it.key == key })
            val v = rpc(3, key = key); ok(v); require(
                DynamicProto.number(
                    v,
                    "revision"
                ) == revision
            ) { "Config changed during fetch" }
            val sourceId = DynamicProto.number(f, "source_id")
            fields += ConfigField(
                key,
                DynamicProto.field(f, "name") as String,
                DynamicProto.field(f, "group") as String,
                DynamicProto.number(f, "type"),
                DynamicProto.decimal(f, "minimum"),
                DynamicProto.decimal(f, "maximum"),
                DynamicProto.field(f, "source") as String,
                DynamicProto.decimal(v, "value"),
                if (sourceId > 0) DynamicProto.decimal(f, "default_value") else null,
                sourceId
            )
            progress(i + 1, total)
        }
        return RobotConfig(fields, revision ?: error("No config fields"), provisioned)
    }

    suspend fun write(field: ConfigField, value: Float, revision: Int): Int {
        validate(field, value)
        val current = rpc(3, key = field.key); ok(current)
        check(
            DynamicProto.number(
                current,
                "revision"
            ) == revision
        ) { "Config changed before write" }
        if (DynamicProto.decimal(current, "value") == value) return revision
        val written = rpc(4, key = field.key, value = value, revision = revision); ok(written)
        val next = DynamicProto.number(written, "revision")
        val verified = rpc(3, key = field.key); ok(verified)
        check(
            DynamicProto.number(verified, "revision") == next && DynamicProto.decimal(
                verified,
                "value"
            ) == value
        ) { "Write verification failed; fetch config" }
        return next
    }

    suspend fun calibrate(revision: Int) {
        ok(rpc(5, revision = revision))
    }

    suspend fun calibrateColor(colorId: Int) {
        require(colorId in 0..3) { "Color slot must be 0..3" }
        val ack = exchange(8, 3, COLOR_CALIBRATION, mapOf("color_id_mapped" to colorId))
        val status = DynamicProto.number(ack, "status")
        check(DynamicProto.number(ack, "operation") == 3 && status == 0) {
            when (status) {
                1 -> "Color sample unavailable, stale, or flash write failed."; 2 -> "Invalid color slot or command."; 3 -> "Robot busy."; 4 -> "SPI bridge is not ready."; else -> "Color calibration rejected (status $status)."
            }
        }
    }

    fun export(config: RobotConfig): String {
        val values = JSONArray(); config.fields.forEach {
            values.put(
                JSONObject().put("key", it.key).put("name", it.name).put("type", it.type)
                    .put("value", it.value)
            )
        }
        return JSONObject().put("format", "ra.remote.config.v1").put("target", "stm32f411ce")
            .put("schema_sha256", schemaHash)
            .put("captured_revision", config.revision.toLong() and 0xffffffffL)
            .put("values", values).toString(2)
    }

    suspend fun restore(text: String, progress: (Int, Int, String) -> Unit): RobotConfig {
        require(text.toByteArray().size <= 262144) { "Backup exceeds 256 KiB" }
        val json = JSONObject(text)
        require(
            json.getString("format") == "ra.remote.config.v1" && json.getString("target") == "stm32f411ce" && json.getString(
                "schema_sha256"
            ) == schemaHash
        ) { "Backup target or protobuf schema does not match" }
        // Fetch all current values and validate every import before the first mutation.
        var current = fetch { i, n -> progress(i, n, "Comparing") }
        val imported = json.getJSONArray("values")
        require(imported.length() <= current.fields.size)
        val desired = linkedMapOf<Int, Float>()
        for (i in 0 until imported.length()) {
            val row = imported.getJSONObject(i)
            val key = row.getInt("key")
            val field = current.fields.singleOrNull { it.key == key } ?: error("Unknown key $key")
            require(
                row.getString("name") == field.name && row.getInt("type") == field.type && !desired.containsKey(
                    key
                )
            ) { "Backup key/type mismatch" }
            val value = row.getDouble("value").toFloat(); validate(field, value); desired[key] =
                value
        }
        // Preflight final reference geometry and RGB before any flash mutation.
        val finalValues = current.fields.associate { it.key to (desired[it.key] ?: it.value) }
        for (base in listOf(100, 110, 120, 130)) if (finalValues.getValue(base) != 0f) {
            require(
                finalValues.getValue(base + 1) + finalValues.getValue(base + 2) + finalValues.getValue(
                    base + 3
                ) > 0f
            ) { "Enabled color reference needs nonzero RGB" }
            require(
                finalValues.getValue(base + 4) != 0f || (finalValues.getValue(base + 5) <= 4f && finalValues.getValue(
                    base + 6
                ) <= 4f)
            ) { "Cell references use grid indices 0..4" }
        }
        // Disable each changed reference, update its components, then restore its enable bit.
        // All mutations remain sequential and each write is skipped if the fetched value matches.
        val ordered = mutableListOf<Pair<Int, Float>>()
        for (base in listOf(100, 110, 120, 130)) {
            val changed = (base..base + 6).any { k ->
                desired[k]?.let { v -> v != current.fields.single { it.key == k }.value } ?: false
            }
            if (changed) ordered += base to 0f
        }
        for ((key, value) in desired) if (key !in listOf(
                100,
                110,
                120,
                130
            )
        ) ordered += key to value
        for (base in listOf(
            100,
            110,
            120,
            130
        )) if (ordered.any { it.first == base }) ordered += base to finalValues.getValue(base)
        var revision = current.revision
        var completed = 0
        try {
            for ((key, value) in ordered) {
                val field = current.fields.single { it.key == key }; revision =
                    write(field, value, revision); completed++; progress(
                    completed,
                    ordered.size,
                    "Restoring ${field.name}"
                )
            }
        } catch (e: Exception) {
            throw IllegalStateException(
                "Restore stopped after $completed / ${ordered.size} steps. Applied keys remain on robot. ${e.message}",
                e
            )
        }
        return fetch { i, n -> progress(i, n, "Verifying") }
    }

    suspend fun source(key: Int = 1): String {
        val bytes = ByteArrayOutputStream()
        var total = -1
        do {
            val r = rpc(6, key = key, offset = bytes.size()); ok(r)
            val chunk = (DynamicProto.field(r, "schema_chunk") as ByteString).toByteArray()
            val size = DynamicProto.number(r, "total")
            require(
                size in 1..65536 && (total < 0 || total == size) && DynamicProto.number(
                    r,
                    "offset"
                ) == bytes.size() && chunk.isNotEmpty() && bytes.size() + chunk.size <= size
            )
            total = size; bytes.write(chunk)
        } while (bytes.size() < total)
        return bytes.toString("UTF-8")
    }

    suspend fun sendJson(kind: Int, type: String, json: String) {
        require(kind == 2 && type == CONTROL) { "Use config editor for mutations; raw console supports Control only" }
        // Raw control still uses request correlation and ACK; JSON adds no hidden retries.
        val m = schema.parse(type, schema.json(type, json)); control(
            DynamicProto.number(
                m,
                "operation"
            )
        )
    }

    fun close() {
        closed = true; link.close(); reader?.cancel(); pending?.third?.completeExceptionally(
            IllegalStateException("Connection closed; operation unconfirmed")
        ); pending =
            null; genericPending?.second?.completeExceptionally(IllegalStateException("Connection closed; command unconfirmed")); genericPending =
            null
    }

    companion object {
        fun validate(f: ConfigField, value: Float) {
            require(value.isFinite() && value >= f.minimum && value <= f.maximum && (f.type == 0 || value % 1f == 0f)) { "${f.name}: expected ${f.minimum}..${f.maximum}${if (f.type != 0) " (integer)" else ""}" }
        }
    }
}

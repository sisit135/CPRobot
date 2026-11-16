// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import android.content.Context
import android.net.Uri
import dalvik.system.DexClassLoader
import dev.ra.remote.proto.DynamicProto
import dev.ra.remote.proto.Frames
import dev.ra.remote.transport.TransportProvider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.security.MessageDigest

data class ProviderSpec(val hash: String, val entry: String, val id: String, val label: String) {
    val key: String get() = "$hash:$entry"
}

/** Imports execute code with this application's permissions. Only explicit local file imports. */
class TransportPlugins(private val context: Context) {
    private val prefs = context.getSharedPreferences("transport-providers", 0)
    private val directory = File(context.filesDir, "transport-providers").apply { mkdirs() }

    fun list(): List<ProviderSpec> = runCatching {
        val rows = JSONArray(prefs.getString("providers", "[]"))
        (0 until rows.length()).map { i -> rows.getJSONObject(i).let {
            ProviderSpec(it.getString("hash"), it.getString("entry"), it.getString("id"), it.getString("label"))
        } }
    }.getOrDefault(emptyList())

    private fun digest(bytes: ByteArray) = MessageDigest.getInstance("SHA-256").digest(bytes)
        .joinToString("") { "%02x".format(it) }

    fun load(spec: ProviderSpec): TransportProvider {
        require(spec.hash.matches(Regex("[0-9a-f]{64}"))) { "Invalid provider identity" }
        val file = File(directory, "${spec.hash}.dex").takeIf { it.exists() } ?: File(directory, "${spec.hash}.jar")
        require(file.isFile && file.length() in 1..MAX_BYTES.toLong()) { "Provider file missing; import again" }
        require(digest(file.readBytes()) == spec.hash) { "Provider file changed; import again" }
        check(file.setReadOnly()) { "Cannot protect provider file" }
        val loader = DexClassLoader(file.absolutePath, context.codeCacheDir.absolutePath, null,
            TransportProvider::class.java.classLoader)
        val provider = loader.loadClass(spec.entry).getDeclaredConstructor().newInstance() as? TransportProvider
            ?: error("Entry class must implement TransportProvider")
        require(provider.apiVersion() == TransportProvider.API_VERSION) { "Unsupported transport API version" }
        require(provider.id().isNotBlank() && provider.label().isNotBlank()) { "Provider has no identity" }
        DynamicProto(provider.descriptorSet())
        return provider
    }

    @Synchronized
    fun import(uri: Uri, entry: String): ProviderSpec {
        require(entry.matches(Regex("[A-Za-z_$][\\w$]*(\\.[A-Za-z_$][\\w$]*)+"))) { "Invalid entry class name" }
        val bytes = context.contentResolver.openInputStream(uri)?.use {
            val out = java.io.ByteArrayOutputStream(); val buffer = ByteArray(8192)
            while (true) {
                val n = it.read(buffer); if (n < 0) break
                require(out.size() + n <= MAX_BYTES) { "Provider exceeds 8 MiB" }
                out.write(buffer, 0, n)
            }
            require(out.size() > 0) { "Empty provider" }; out.toByteArray()
        } ?: error("Cannot open provider")
        val hash = digest(bytes)
        val dex = bytes.size >= 4 && bytes.copyOfRange(0, 4).contentEquals(byteArrayOf(100, 101, 120, 10))
        val zip = bytes.size >= 4 && bytes[0] == 80.toByte() && bytes[1] == 75.toByte()
        require(dex || zip) { "Select a DEX or an archive containing classes.dex" }
        val file = File(directory, "$hash.${if (dex) "dex" else "jar"}")
        if (!file.exists()) {
            // Android 14+: mark read-only immediately after opening, before writing through the fd.
            FileOutputStream(file).use { stream ->
                check(file.setReadOnly()) { "Cannot protect provider file" }
                stream.write(bytes); stream.fd.sync()
            }
        }
        val draft = ProviderSpec(hash, entry, "", "")
        val provider = load(draft)
        val spec = draft.copy(id = provider.id(), label = provider.label())
        val rows = JSONArray()
        (list().filterNot { it.hash == hash && it.entry == entry } + spec).forEach {
            rows.put(JSONObject().put("hash", it.hash).put("entry", it.entry).put("id", it.id).put("label", it.label))
        }
        prefs.edit().putString("providers", rows.toString()).apply()
        return spec
    }

    companion object { const val MAX_BYTES = 8 * 1024 * 1024 }
}

class ProviderLink(private val connection: TransportProvider.Connection, override val label: String) : Link {
    override suspend fun open() = withContext(Dispatchers.IO) { connection.open() }
    override suspend fun read(): Frames.Frame = withContext(Dispatchers.IO) {
        connection.read().let { Frames.Frame(it.kind, it.payload) }
    }
    override suspend fun send(kind: Int, bytes: ByteArray) = withContext(Dispatchers.IO) { connection.send(kind, bytes) }
    override fun close() = connection.close()
}

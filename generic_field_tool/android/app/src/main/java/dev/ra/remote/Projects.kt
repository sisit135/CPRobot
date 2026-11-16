// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import android.content.SharedPreferences
import android.util.Base64
import dev.ra.remote.proto.DynamicProto
import org.json.JSONArray
import org.json.JSONObject
import java.util.UUID

data class ToolProject(
    val id: String = UUID.randomUUID().toString(),
    val name: String,
    val schema: String = "",
    val settings: ConnectionSettings = ConnectionSettings(),
    val controls: List<ControlButton> = emptyList()
)

class ProjectStore(private val prefs: SharedPreferences) {
    fun load(): List<ToolProject> = runCatching {
        val rows = JSONArray(prefs.getString("projects", "[]"))
        (0 until rows.length()).map { i ->
            val p = rows.getJSONObject(i); ToolProject(
            p.getString("id"),
            p.getString("name"),
            p.optString("schema"),
            ConnectionSettings(
                p.optString("mode", "Auto"),
                p.optString("host"),
                p.optString("pin"),
                p.optString("ble"),
                p.optString("provider"),
                p.optString("providerOptions", "{}")
            ),
            if (p.has("controls")) ControlLayout.decode(p.getString("controls")) else emptyList()
        )
        }
    }.getOrDefault(emptyList())

    fun save(projects: List<ToolProject>, selected: String?) {
        val rows = JSONArray(); projects.forEach { p ->
            rows.put(
                JSONObject().put("id", p.id).put("name", p.name).put("schema", p.schema)
                    .put("mode", p.settings.mode).put("host", p.settings.host)
                    .put("pin", p.settings.pin).put("ble", p.settings.ble)
                    .put("provider", p.settings.provider).put("providerOptions", p.settings.providerOptions)
                    .put("controls", ControlLayout.encode(p.controls))
            )
        }
        prefs.edit().putString("projects", rows.toString()).putString("selected", selected).apply()
    }

    companion object {
        fun validate(bytes: ByteArray): DynamicProto {
            val schema = DynamicProto(bytes); require(
                schema.names().isNotEmpty()
            ) { "Schema has no messages" }; return schema
        }

        fun decode(project: ToolProject) = validate(Base64.decode(project.schema, Base64.DEFAULT))
    }
}

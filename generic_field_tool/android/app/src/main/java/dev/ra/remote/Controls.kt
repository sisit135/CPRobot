// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import dev.ra.remote.proto.DynamicProto
import org.json.JSONArray
import org.json.JSONObject
import java.util.UUID

enum class HorizontalAnchor { LEFT, RIGHT }
enum class VerticalAnchor { TOP, BOTTOM }
enum class ControlShape { RECTANGLE, ROUNDED, ELLIPSE }

data class ControlButton(
    val id: String = UUID.randomUUID().toString(),
    val label: String = "Button",
    val request: String = "",
    val parameters: String = "{}",
    val horizontal: HorizontalAnchor = HorizontalAnchor.LEFT,
    val horizontalOffset: Float = 16f,
    val vertical: VerticalAnchor = VerticalAnchor.TOP,
    val verticalOffset: Float = 16f,
    val width: Float = 96f,
    val height: Float = 64f,
    val shape: ControlShape = ControlShape.ROUNDED,
    val repeatMs: Int = 0,
    val releaseParameters: String? = null
) {
    /** Offset is measured from the selected physical edge to the button's corresponding edge. */
    fun position(viewWidth: Float, viewHeight: Float): Pair<Float, Float> =
        (if (horizontal == HorizontalAnchor.LEFT) horizontalOffset else viewWidth - width - horizontalOffset) to
            (if (vertical == VerticalAnchor.TOP) verticalOffset else viewHeight - height - verticalOffset)

    fun validate(schema: DynamicProto? = null) {
        require(id.isNotBlank() && id.length <= 100 && label.isNotBlank() && label.length <= 80) { "Invalid button identity/label" }
        require(request.substringAfterLast('.').endsWith("Request")) { "Bind a protobuf *Request" }
        require(horizontalOffset.isFinite() && horizontalOffset in 0f..10000f &&
            verticalOffset.isFinite() && verticalOffset in 0f..10000f) { "Offsets must be 0–10000 dp" }
        require(width.isFinite() && width in 32f..1000f && height.isFinite() && height in 32f..1000f) { "Size must be 32–1000 dp" }
        require(repeatMs == 0 || repeatMs in 60..2000) { "Repeat must be 0 or 60–2000 ms" }
        require(repeatMs == 0 || releaseParameters != null) { "Repeating controls require release parameters" }
        for (json in listOfNotNull(parameters, releaseParameters)) {
            require(json.length <= 16384) { "Binding JSON too large" }; JSONObject(json)
            schema?.json(request, json)
        }
    }

    fun json(): JSONObject = JSONObject().put("id", id).put("label", label)
        .put("request", request).put("parameters", JSONObject(parameters))
        .put("horizontal", JSONObject().put("side", horizontal.name).put("offsetDp", horizontalOffset))
        .put("vertical", JSONObject().put("side", vertical.name).put("offsetDp", verticalOffset))
        .put("widthDp", width).put("heightDp", height).put("shape", shape.name)
        .put("repeatMs", repeatMs).also { json ->
            releaseParameters?.let { json.put("releaseParameters", JSONObject(it)) }
        }

    companion object {
        fun fromJson(json: JSONObject): ControlButton {
            val x = json.getJSONObject("horizontal"); val y = json.getJSONObject("vertical")
            return ControlButton(json.getString("id"), json.getString("label"), json.getString("request"),
                json.getJSONObject("parameters").toString(), HorizontalAnchor.valueOf(x.getString("side")),
                x.getDouble("offsetDp").toFloat(), VerticalAnchor.valueOf(y.getString("side")),
                y.getDouble("offsetDp").toFloat(), json.getDouble("widthDp").toFloat(),
                json.getDouble("heightDp").toFloat(), ControlShape.valueOf(json.getString("shape")),
                json.optInt("repeatMs", 0), json.optJSONObject("releaseParameters")?.toString()).also { it.validate() }
        }

        fun at(x: Float, y: Float, viewWidth: Float, viewHeight: Float): ControlButton {
            val w = minOf(96f, viewWidth.coerceAtLeast(32f)); val h = minOf(64f, viewHeight.coerceAtLeast(32f))
            val left = (x - w / 2).coerceIn(0f, (viewWidth - w).coerceAtLeast(0f))
            val top = (y - h / 2).coerceIn(0f, (viewHeight - h).coerceAtLeast(0f))
            val right = x > viewWidth / 2; val bottom = y > viewHeight / 2
            return ControlButton(horizontal = if (right) HorizontalAnchor.RIGHT else HorizontalAnchor.LEFT,
                horizontalOffset = if (right) viewWidth - left - w else left,
                vertical = if (bottom) VerticalAnchor.BOTTOM else VerticalAnchor.TOP,
                verticalOffset = if (bottom) viewHeight - top - h else top, width = w, height = h)
        }
    }
}

object ControlLayout {
    const val MAX_BYTES = 262144
    fun encode(buttons: List<ControlButton>): String {
        require(buttons.size <= 100) { "Maximum 100 controls" }
        val rows = JSONArray(); buttons.forEach { it.validate(); rows.put(it.json()) }
        return JSONObject().put("format", "field-tool-controls").put("version", 1).put("controls", rows).toString(2)
            .also { require(it.toByteArray().size <= MAX_BYTES) { "Layout exceeds 256 KiB" } }
    }
    fun decode(text: String, schema: DynamicProto? = null): List<ControlButton> {
        require(text.toByteArray().size <= MAX_BYTES) { "Layout exceeds 256 KiB" }
        val root = JSONObject(text)
        require(root.getString("format") == "field-tool-controls" && root.getInt("version") == 1) { "Unsupported control layout" }
        val rows = root.getJSONArray("controls"); require(rows.length() <= 100) { "Maximum 100 controls" }
        val buttons = (0 until rows.length()).map { ControlButton.fromJson(rows.getJSONObject(it)).also { b -> b.validate(schema) } }
        require(buttons.map { it.id }.distinct().size == buttons.size) { "Duplicate control IDs" }
        return buttons
    }
}

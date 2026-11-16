// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

enum class VideoTracePatternMode { GLOB, REGEX }

data class VideoTracingSettings(
    val enabled: Boolean = false,
    val pattern: String = "",
    val mode: VideoTracePatternMode = VideoTracePatternMode.GLOB
)

data class VideoTraceHost(val address: String, val name: String)
data class VideoTracingStatus(
    val message: String = "Disabled",
    val connected: Boolean = false,
    val armed: Boolean = false,
    val hosts: List<VideoTraceHost> = emptyList(),
    val lastTrigger: String? = null
)

/** Regex searches the message; glob matches the whole message with * and ? wildcards. */
class VideoTracePattern(settings: VideoTracingSettings) {
    private val mode = settings.mode
    private val expression: Regex

    init {
        require(settings.pattern.isNotBlank()) { "Enter a log pattern" }
        require(settings.pattern.length <= 256) { "Use at most 256 characters" }
        expression = when (mode) {
            VideoTracePatternMode.REGEX -> Regex(settings.pattern)
            VideoTracePatternMode.GLOB -> Regex(buildString {
                append("\\A")
                settings.pattern.forEach { c ->
                    append(
                        when (c) {
                            '*' -> "[\\s\\S]*"; '?' -> "[\\s\\S]"; else -> Regex.escape(c.toString())
                        }
                    )
                }
                append("\\z")
            })
        }
    }

    fun matches(message: String) = when (mode) {
        VideoTracePatternMode.REGEX -> expression.containsMatchIn(message)
        VideoTracePatternMode.GLOB -> expression.matches(message)
    }
}

/** One matching attempt consumes the arm, including an unconfirmed send. */
class VideoTraceTrigger(
    settings: VideoTracingSettings,
    initiallyArmed: Boolean = settings.enabled
) {
    private val pattern = VideoTracePattern(settings)
    var armed = settings.enabled && initiallyArmed
        private set

    fun arm() {
        armed = true
    }

    fun onLog(message: String, sendPress: () -> Boolean): Boolean {
        if (!armed || !pattern.matches(message)) return false
        armed = false
        return sendPress()
    }
}

// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import android.app.Application
import android.Manifest
import android.content.*
import android.content.pm.PackageManager
import android.os.Build
import android.os.IBinder
import androidx.core.content.ContextCompat
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.google.protobuf.DynamicMessage
import dev.ra.remote.proto.DynamicProto
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import java.util.concurrent.atomic.AtomicLong
import org.json.JSONObject
import android.util.Base64

data class ConnectionSettings(
    val mode: String = "Auto",
    val host: String = "",
    val pin: String = "",
    val ble: String = "",
    val provider: String = "",
    val providerOptions: String = "{}"
)

data class LogLine(
    val id: Long,
    val time: String,
    val level: String,
    val tag: String,
    val text: String,
    val robotTimeMs: Long? = null
)

data class RemoteState(
    val connection: String = "Disconnected",
    val stage: String = "",
    val connected: Boolean = false,
    val connecting: Boolean = false,
    val settings: ConnectionSettings = ConnectionSettings(),
    val config: RobotConfig? = null,
    val pose: String = "Waiting for telemetry",
    val schemaTypes: List<String> = emptyList(),
    val providers: List<ProviderSpec> = emptyList(),
    val controlEditing: Boolean = false,
    val controlEditor: ControlButton? = null,
    val busy: String? = null,
    val progress: Int = 0,
    val total: Int = 0,
    val projects: List<ToolProject> = emptyList(),
    val selectedProject: String? = null,
    val sensors: Map<String, String> = emptyMap(),
    val sensorPolling: Boolean = true,
    val logPolling: Boolean = true,
    val events: List<LogLine> = emptyList(),
    val source: String? = null,
    val logs: List<LogLine> = emptyList(),
    val error: String? = null,
    val notice: String? = null,
    val videoTracing: VideoTracingSettings = VideoTracingSettings(),
    val videoTracingStatus: VideoTracingStatus = VideoTracingStatus()
)

class RemoteModel(app: Application) : AndroidViewModel(app) {
    private val prefs = app.getSharedPreferences("connection", 0)
    private val _state = MutableStateFlow(
        RemoteState(
            settings = ConnectionSettings(
                prefs.getString(
                    "mode",
                    "Auto"
                )!!,
                prefs.getString("host", "")!!,
                prefs.getString("pin", "")!!,
                prefs.getString("ble", "")!!
            ), videoTracing = loadVideoTracingSettings(app)
        )
    )
    private val projectStore = ProjectStore(prefs)
    private val plugins = TransportPlugins(app)
    val state = _state.asStateFlow()

    init {
        val projects = projectStore.load()
        val selected =
            projects.find { it.id == prefs.getString("selected", null) } ?: projects.firstOrNull()
        _state.update {
            it.copy(
                projects = projects,
                selectedProject = selected?.id,
                settings = selected?.settings ?: it.settings,
                providers = plugins.list()
            )
        }
    }

    private var providerSchema: Pair<String, DynamicProto>? = null
    fun projectSchema(): DynamicProto? {
        val s = state.value
        if (s.settings.provider.isNotEmpty()) {
            val spec = s.providers.firstOrNull { it.key == s.settings.provider } ?: return null
            if (providerSchema?.first != spec.key) providerSchema = spec.key to DynamicProto(plugins.load(spec).descriptorSet())
            return providerSchema?.second
        }
        return s.projects.find { it.id == s.selectedProject }?.takeIf { it.schema.isNotEmpty() }?.let(ProjectStore::decode)
            ?: session?.schema?.takeIf { s.connected }
    }

    fun importProvider(uri: android.net.Uri, entry: String) {
        val projectId = state.value.selectedProject
        viewModelScope.launch {
            try {
                val spec = withContext(Dispatchers.IO) { plugins.import(uri, entry.trim()) }
                _state.update { it.copy(providers = plugins.list()) }
                if (state.value.selectedProject == projectId) settings(state.value.settings.copy(provider = spec.key))
                else {
                    _state.update { state -> state.copy(projects = state.projects.map {
                        if (it.id == projectId) it.copy(settings = it.settings.copy(provider = spec.key)) else it
                    }) }; saveProjects()
                }
            } catch (e: Exception) { _state.update { it.copy(error = e.message ?: "Provider import failed") } }
        }
    }

    fun editControls(editing: Boolean) {
        stopControls(); _state.update { it.copy(controlEditing = editing, controlEditor = null) }
    }
    fun editControl(button: ControlButton?) { stopControls(); _state.update { it.copy(controlEditor = button) } }
    fun saveControl(button: ControlButton) {
        val schema = projectSchema() ?: error("Select a provider or import a schema first")
        button.validate(schema)
        updateControls { rows -> rows.filterNot { it.id == button.id } + button }
        _state.update { it.copy(controlEditor = null) }
    }
    fun deleteControl(id: String) { stopControls(); updateControls { it.filterNot { b -> b.id == id } }; editControl(null) }
    private fun updateControls(update: (List<ControlButton>) -> List<ControlButton>) {
        _state.update { state -> state.copy(projects = state.projects.map {
            if (it.id == state.selectedProject) it.copy(controls = update(it.controls).also(ControlLayout::encode)) else it
        }) }; saveProjects()
    }
    fun importControls(text: String) {
        val schema = projectSchema() ?: error("Select a provider or import a schema first")
        val buttons = ControlLayout.decode(text, schema)
        stopControls(); updateControls { buttons }; editControl(null)
    }
    fun exportControls() = ControlLayout.encode(state.value.projects.find { it.id == state.value.selectedProject }?.controls.orEmpty())

    private val controlJobs = mutableMapOf<String, Job>()
    fun pressControl(button: ControlButton) {
        if (state.value.controlEditing || state.value.controlEditor != null || !state.value.connected || controlJobs.containsKey(button.id)) return
        val current = session ?: return
        val job = viewModelScope.launch(start = CoroutineStart.LAZY) {
            try {
                do {
                    val response = current.sendCommand(button.request, button.parameters)
                    if (session === current) event(button.request, response)
                    if (button.repeatMs == 0) break
                    delay(button.repeatMs.toLong())
                } while (isActive && session === current && state.value.connected)
            } catch (e: CancellationException) { throw e }
            catch (e: Exception) { event(button.request, "Unconfirmed: ${e.message}") }
            finally {
                if (button.repeatMs > 0 && button.releaseParameters != null && session === current && state.value.connected) {
                    withContext(NonCancellable) {
                        runCatching { current.sendCommand(button.request, button.releaseParameters) }
                            .onSuccess { event(button.request, it) }
                            .onFailure { event(button.request, "Release unconfirmed: ${it.message}") }
                    }
                }
                controlJobs.remove(button.id)
            }
        }
        controlJobs[button.id] = job
        job.start()
    }
    fun releaseControl(id: String) { controlJobs[id]?.cancel() }
    fun stopControls() { controlJobs.values.toList().forEach { it.cancel() } }

    private fun saveProjects() {
        projectStore.save(state.value.projects, state.value.selectedProject)
    }

    fun addProject(name: String) {
        if (name.isBlank()) return
        val p = ToolProject(name = name.trim()); cancelConnect(); _state.update {
            it.copy(
                projects = it.projects + p,
                selectedProject = p.id,
                settings = p.settings,
                events = emptyList(),
                logs = emptyList(),
                schemaTypes = emptyList()
            )
        }; saveProjects()
    }

    fun selectProject(id: String) {
        if (id == state.value.selectedProject) return
        val p = state.value.projects.firstOrNull { it.id == id }
            ?: return; cancelConnect(); _state.update {
            it.copy(
                selectedProject = id,
                settings = p.settings,
                events = emptyList(),
                logs = emptyList(),
                schemaTypes = if (p.schema.isEmpty()) emptyList() else ProjectStore.decode(p)
                    .names().sorted()
            )
        }; saveProjects()
    }

    fun removeProject(id: String) {
        cancelConnect(); _state.update {
            it.copy(
                projects = it.projects.filter { p -> p.id != id },
                selectedProject = null,
                schemaTypes = emptyList(),
                events = emptyList(),
                logs = emptyList()
            )
        }; state.value.projects.firstOrNull()?.let { selectProject(it.id) }; saveProjects()
    }

    fun renameProject(id: String, name: String) {
        if (name.isBlank()) return; _state.update {
            it.copy(projects = it.projects.map { p ->
                if (p.id == id) p.copy(
                    name = name.trim()
                ) else p
            })
        }; saveProjects()
    }

    fun importSchema(id: String, bytes: ByteArray) {
        val schema = ProjectStore.validate(bytes); cancelConnect(); _state.update {
            it.copy(projects = it.projects.map { p ->
                if (p.id == id) p.copy(
                    schema = Base64.encodeToString(bytes, Base64.NO_WRAP)
                ) else p
            },
                schemaTypes = if (it.selectedProject == id) schema.names()
                    .sorted() else it.schemaTypes
            )
        }; saveProjects()
    }

    fun sensorPolling() {
        _state.update { it.copy(sensorPolling = !it.sensorPolling) }
    }

    fun logPolling() {
        _state.update { it.copy(logPolling = !it.logPolling) }
    }

    private fun event(tag: String, text: String) {
        val row = LogLine(
            ids.incrementAndGet(),
            java.time.LocalTime.now().toString().take(12),
            "I",
            tag,
            text.take(512)
        ); _state.update { it.copy(events = (it.events + row).takeLast(500)) }
    }

    private var session: RobotSession? = null
    private var connectionJob: Job? = null
    private var actionJob: Job? = null
    private val ids = AtomicLong(0)
    fun log(level: String, tag: String, text: String) {
        val entry = LogLine(
            ids.incrementAndGet(),
            java.time.LocalTime.now().toString().take(12),
            level,
            tag,
            text.take(8192)
        ); _state.update { it.copy(logs = (it.logs + entry).takeLast(2000)) }
    }

    fun settings(s: ConnectionSettings) {
        if (s.provider != state.value.settings.provider) { cancelConnect(); providerSchema = null }
        _state.update {
            it.copy(
                settings = s,
                projects = it.projects.map { p -> if (p.id == it.selectedProject) p.copy(settings = s) else p })
        }; saveProjects(); prefs.edit().putString("mode", s.mode).putString("host", s.host)
            .putString("pin", s.pin).putString("ble", s.ble).apply()
    }

    fun connect(bleOnly: Boolean = false) {
        connectionJob?.cancel(); session?.close(); session = null; actionJob?.cancel()
        _state.update {
            it.copy(
                connecting = true,
                connected = false,
                config = null,
                error = null,
                busy = null,
                stage = "Opening transport"
            )
        }
        connectionJob = viewModelScope.launch {
            val s = _state.value.settings
            val provider = try {
                if (s.provider.isEmpty()) null else {
                    val spec = state.value.providers.firstOrNull { it.key == s.provider }
                        ?: error("Import the selected transport provider again")
                    withContext(Dispatchers.IO) { plugins.load(spec) }
                }
            } catch (e: Exception) {
                _state.update { it.copy(connecting = false, error = e.message ?: "Provider failed") }; return@launch
            }
            val bootstrap = if (provider != null) DynamicProto(provider.descriptorSet()) else DynamicProto(
                getApplication<Application>().assets.open("robot.desc").use { it.readBytes() })
            val linkSettings = if (bleOnly) s.copy(mode = "BLE") else s
            val links = try { listOf<Link>(if (provider != null) {
                val json = JSONObject(s.providerOptions)
                val options = mapOf("ble" to s.ble, "host" to s.host, "pin" to s.pin) + json.keys().asSequence().associateWith { json.getString(it) }
                ProviderLink(provider.create(getApplication(), options), provider.label())
            } else RobotLink(
                    getApplication(),
                    linkSettings,
                    bootstrap
                ) { stage -> _state.update { it.copy(stage = stage) } }) }
            catch (e: Exception) {
                _state.update { it.copy(connecting = false, error = e.message ?: "Provider failed") }; return@launch
            }
            for (link in links) {
                ensureActive(); _state.update { it.copy(stage = "Connecting · ${link.label}") }
                lateinit var active: RobotSession
                active = RobotSession(link, bootstrap, viewModelScope, { kind, text ->
                    if (session === active) {
                        if (kind == 1 && state.value.sensorPolling) _state.update { it.copy(pose = text) }
                        if (kind == 3) event("Response", text)
                        if (kind == 11 && state.value.sensorPolling) _state.update {
                            it.copy(
                                sensors = it.sensors + (text.substringBefore(
                                    '\n'
                                ) to text.substringAfter('\n'))
                            )
                        }
                        if (kind == 255) _state.update { it.copy(connection = link.label) }
                        if (kind == 7) {
                            val record = runCatching { JSONObject(text) }.getOrNull()
                            if (record != null) {
                                val level = when (record.optInt("level")) {
                                    0 -> "E"; 1 -> "W"; 2 -> "I"; else -> "D"
                                }
                                robotLog(
                                    level,
                                    record.optString("source", "RCP"),
                                    record.optString("message"),
                                    if (record.has("timestamp_ms")) record.optLong("timestamp_ms") else null
                                )
                            } else if (state.value.logPolling) log("W", "RCP logger", text)
                        } else if (kind == 255) log(
                            if (kind == 255) "W" else if (kind == 3) "I" else "D",
                            if (kind == 255) "Link" else if (kind == 1) "EKF" else "RPC",
                            text
                        )
                    }
                }, { e ->
                    if (session === active) {
                        event(
                            "Link",
                            "Connection lost; pending commands are unconfirmed"
                        ); _state.update {
                            it.copy(
                                connected = false,
                                connection = "Disconnected",
                                error = "Link lost. Any pending write is unconfirmed. ${e.message}",
                                busy = null,
                                config = null,
                                pose = "—",
                                sensors = emptyMap()
                            )
                        }; log("E", "Link", e.message ?: "Disconnected"); active.close(); session =
                            null

                    }
                })
                session = active
                try {
                    active.projectSchema = if (provider != null) bootstrap else
                        state.value.projects.find { it.id == state.value.selectedProject }?.takeIf { it.schema.isNotEmpty() }?.let(ProjectStore::decode)
                    active.start(loadRemoteSchema = active.projectSchema == null) { stage ->
                        _state.update {
                            it.copy(
                                stage = stage
                            )
                        }
                    }
                    val config = if (active.projectSchema == null) active.fetch { i, n ->
                        _state.update {
                            it.copy(stage = "Fetching config · $i / $n")
                        }
                    } else null
                    _state.update {
                        it.copy(
                            connected = true,
                            connecting = false,
                            connection = link.label,
                            config = config,
                            schemaTypes = (active.projectSchema ?: active.schema).names().sorted(),
                            error = null
                        )
                    }
                    log(
                        "I",
                        "Link",
                        "Connected via ${link.label}; protobuf schema loaded"
                    ); return@launch
                } catch (e: TimeoutCancellationException) {
                    active.close(); if (session === active) session = null; log(
                        "W",
                        "Link",
                        "Connection timed out"
                    ); _state.update { it.copy(error = "Connection timed out. Check robot power and the physical pairing window.") }
                } catch (e: CancellationException) {
                    active.close(); throw e
                } catch (e: Exception) {
                    active.close(); if (session === active) session = null; log(
                        "W",
                        "Link",
                        "${link.label}: ${e.message}"
                    ); _state.update { it.copy(error = e.message) }
                }
            }
            _state.update {
                it.copy(
                    connecting = false,
                    connected = false,
                    connection = "Disconnected"
                )
            }
        }
    }

    fun cancelConnect() {
        stopControls()
        connectionJob?.cancel(); actionJob?.cancel(); session?.close(); session =
            null; _state.update {
            it.copy(
                connecting = false,
                connected = false,
                connection = "Disconnected",
                config = null,
                pose = "—",
                sensors = emptyMap(),
                busy = null,
                source = null,
                controlEditor = null
            )
        }
    }

    private fun action(label: String, body: suspend (RobotSession) -> Unit) {
        if (actionJob?.isActive == true) return
        val current = session; if (current == null || !_state.value.connected) {
            _state.update { it.copy(error = "Connect to the robot first") }; return
        }
        actionJob = viewModelScope.launch {
            _state.update { it.copy(busy = label, progress = 0, total = 0, error = null) }
            try {
                body(current)
            } catch (e: TimeoutCancellationException) {
                _state.update { it.copy(error = "No response before timeout; operation unconfirmed. Fetch before sending again.") }; log(
                    "E",
                    "Link",
                    "Operation timed out; no automatic retry"
                )
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                _state.update { it.copy(error = e.message ?: "Operation failed") }; log(
                    "E",
                    "Config",
                    e.message ?: "Failed"
                )
            } finally {
                _state.update { it.copy(busy = null) }
            }
        }
    }

    private fun progress(i: Int, n: Int, label: String) {
        _state.update { it.copy(progress = i, total = n, busy = label) }
    }

    private suspend fun refresh(s: RobotSession) {
        val c = s.fetch { i, n -> progress(i, n, "Fetching config") }; _state.update {
            it.copy(
                config = c
            )
        }
    }

    fun refresh() {
        action("Fetching config") { refresh(it) }
    }

    fun write(f: ConfigField, value: Float) {
        action("Comparing ${f.name}") { s ->
            val revision = _state.value.config?.revision ?: error("Fetch config first"); s.write(
            f,
            value,
            revision
        ); refresh(s); event(
            "Config",
            "Committed and verified ${f.name} = $value"
        ); _state.update { it.copy(notice = "Committed ${f.name} to flash and verified") }
        }
    }

    fun calibrate() {
        action("Sampling stationary gyro") { s ->
            val revision = _state.value.config?.revision
                ?: error("Fetch config first"); s.calibrate(revision); refresh(s); _state.update {
            it.copy(
                notice = "Gyro calibration saved and verified"
            )
        }
        }
    }

    fun calibrateColor(colorId: Int) {
        action("Sampling color slot $colorId") { s ->
            s.calibrateColor(colorId); refresh(s); _state.update {
            it.copy(
                notice = "Color slot $colorId saved to RCP flash configuration"
            )
        }
        }
    }

    fun restore(text: String) {
        action("Comparing backup") { s ->
            val c = s.restore(text) { i, n, l -> progress(i, n, l) }; _state.update {
            it.copy(
                config = c,
                notice = "Restore complete; values verified"
            )
        }
        }
    }

    fun export(write: suspend (String) -> Unit) {
        action("Fetching export snapshot") { s ->
            val c = s.fetch { i, n -> progress(i, n, "Fetching export snapshot") }; _state.update {
            it.copy(
                config = c
            )
        }; write(s.export(c)); _state.update { it.copy(notice = "Config exported") }
        }
    }

    fun provision(ssid: String, password: String) {
        action("Saving WLAN credentials") { s ->
            val ble = s.link as? RobotLink ?: error("BLE session required"); ble.provision(
            ssid,
            password
        ); _state.update { it.copy(notice = "WLAN settings sent to ESP32") }
        }
    }

    fun command(command: String) {
        val text = command.trim(); if (text.isEmpty()) return
        log("I", "Console", "> $text")
        when {
            text == "clear" -> _state.update { it.copy(logs = emptyList()) }
            text == "help" -> log(
                "I",
                "Help",
                "start | halt | calibrate-color <0..3> | get <key> | set <key> <value> | types | source | send 2 axi.telemetry.v1.Control {\"operation\":1}"
            )

            text == "types" -> log("I", "Schema", _state.value.schemaTypes.joinToString("\n"))
            text == "start" || text == "halt" -> action("Sending $text") { s ->
                s.control(if (text == "start") 1 else 2); log(
                "I",
                "RPC",
                "$text acknowledged by RCP"
            )
            }

            text == "source" -> source()
            text.startsWith("calibrate-color ") -> text.substringAfter(' ').toIntOrNull()
                ?.takeIf { it in 0..3 }?.let(::calibrateColor) ?: log(
                "E",
                "Console",
                "Usage: calibrate-color <slot 0..3>"
            )

            text.startsWith("get ") -> action("Reading value") { s ->
                val key = text.substringAfter(' ').toInt()
                val r = s.rpc(3, key = key); check(DynamicProto.number(r, "status") == 0); log(
                "I",
                "Config",
                s.schema.json(r)
            )
            }

            text.startsWith("set ") -> {
                val parts = text.split(Regex("\\s+"))
                val field = parts.getOrNull(1)?.toIntOrNull()
                    ?.let { k -> _state.value.config?.fields?.find { it.key == k } }
                val value =
                    parts.getOrNull(2)?.toFloatOrNull(); if (field != null && value != null) write(
                    field,
                    value
                ) else log("E", "Console", "Usage: set <known-key> <number>")
            }

            text.startsWith("send ") -> action("Sending protobuf") { s ->
                val p = text.split(' ', limit = 4); require(p.size == 4); s.sendJson(
                p[1].toInt(),
                p[2],
                p[3]
            ); log("I", "RPC", "Command acknowledged")
            }

            else -> log("W", "Console", "Unknown command; type help")
        }
    }

    fun sendCommand(type: String, json: String) {
        action("Sending $type") { s ->
            try {
                val response = s.sendCommand(type, json); if (session === s) event(type, response)
            } catch (e: TimeoutCancellationException) {
                if (session === s) event(type, "Unconfirmed: response timed out"); throw e
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                if (session === s) event(type, "Unconfirmed: ${e.message}"); throw e
            }
        }
    }

    fun source(key: Int = 1) {
        action("Fetching source from RCP") { s -> _state.update { it.copy(source = s.source(key)) } }
    }

    fun closeSource() {
        _state.update { it.copy(source = null) }
    }

    fun dismissError() {
        _state.update { it.copy(error = null, notice = null) }
    }

    private var videoService: VideoTracingService? = null
    private var videoBound = false
    private var videoStatusJob: Job? = null
    private var videoEventsJob: Job? = null
    private val videoConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName, binder: IBinder) {
            if (Build.VERSION.SDK_INT < 28 || !videoBound || !state.value.videoTracing.enabled) return
            val service = (binder as VideoTracingService.LocalBinder).service
            videoService = service
            videoStatusJob?.cancel(); videoEventsJob?.cancel()
            videoStatusJob = viewModelScope.launch {
                service.status.collect { status ->
                    _state.update {
                        it.copy(videoTracingStatus = status)
                    }
                }
            }
            videoEventsJob = viewModelScope.launch {
                service.events.collect { message ->
                    log(
                        "I",
                        "Video tracing",
                        message
                    )
                }
            }
        }

        override fun onServiceDisconnected(name: ComponentName) {
            videoService = null; videoStatusJob?.cancel(); videoEventsJob?.cancel()
            _state.update { it.copy(videoTracingStatus = VideoTracingStatus(message = "Video tracing service disconnected")) }
        }
    }

    private fun robotLog(level: String, tag: String, message: String, robotTimeMs: Long?) {
        val row = LogLine(
            ids.incrementAndGet(),
            java.time.LocalTime.now().toString().take(12),
            level,
            tag,
            message.take(8192),
            robotTimeMs
        )
        if (state.value.logPolling) _state.update { it.copy(logs = (it.logs + row).takeLast(2000)) }
        // Reader callbacks run on IO. HID callbacks, arming and key release are serialized on main.
        viewModelScope.launch { if (Build.VERSION.SDK_INT >= 28) videoService?.onRobotLog(row) }
    }

    fun videoTracing(settings: VideoTracingSettings): Boolean {
        if (settings.enabled) {
            if (Build.VERSION.SDK_INT < 28) {
                _state.update { it.copy(error = "Video tracing needs Android 9 or newer") }; return false
            }
            val error = runCatching { VideoTracePattern(settings) }.exceptionOrNull()
            if (error != null) {
                _state.update { it.copy(error = error.message ?: "Invalid pattern") }; return false
            }
            if (Build.VERSION.SDK_INT >= 31 && getApplication<Application>().checkSelfPermission(
                    Manifest.permission.BLUETOOTH_CONNECT
                ) != PackageManager.PERMISSION_GRANTED
            ) {
                _state.update { it.copy(error = "Allow Nearby devices permission to use the Bluetooth shutter") }; return false
            }
        }
        val editor = prefs.edit().putBoolean("video.enabled", settings.enabled)
            .putString("video.pattern", settings.pattern)
            .putString("video.mode", settings.mode.name)
        if (!settings.enabled || !state.value.videoTracing.enabled) editor.putBoolean(
            "video.armed",
            settings.enabled
        )
        editor.apply()
        _state.update { it.copy(videoTracing = settings) }
        if (settings.enabled) resumeVideoTracing() else stopVideoTracing()
        return true
    }

    fun resumeVideoTracing() {
        if (!state.value.videoTracing.enabled || Build.VERSION.SDK_INT < 28) return
        val app = getApplication<Application>()
        if (Build.VERSION.SDK_INT >= 31 && app.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
            _state.update { it.copy(videoTracingStatus = VideoTracingStatus(message = "Nearby devices permission required; open Video tracing")) }; return
        }
        runCatching {
            val intent = Intent(app, VideoTracingService::class.java)
            ContextCompat.startForegroundService(app, intent)
            if (!videoBound) videoBound =
                app.bindService(intent, videoConnection, Context.BIND_AUTO_CREATE)
            check(videoBound) { "Unable to bind video tracing service" }
        }.onFailure { failure ->
            _state.update {
                it.copy(
                    videoTracingStatus = VideoTracingStatus(
                        message = "Cannot start Bluetooth shutter: ${failure.message}"
                    )
                )
            }
        }
    }

    fun armVideoTracing() {
        if (Build.VERSION.SDK_INT >= 28) videoService?.arm()
    }

    fun connectVideoPhone(address: String) {
        if (Build.VERSION.SDK_INT >= 28) videoService?.connectPhone(address)
    }

    private fun stopVideoTracing() {
        videoStatusJob?.cancel(); videoEventsJob?.cancel(); videoService = null
        val app = getApplication<Application>()
        if (videoBound) {
            app.unbindService(videoConnection); videoBound = false
        }
        if (Build.VERSION.SDK_INT >= 28) app.stopService(
            Intent(
                app,
                VideoTracingService::class.java
            )
        )
        _state.update { it.copy(videoTracingStatus = VideoTracingStatus()) }
    }

    override fun onCleared() {
        session?.close(); stopVideoTracing(); super.onCleared()
    }
}

// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import android.Manifest
import android.os.Build
import android.os.Bundle
import android.widget.LinearLayout
import android.widget.TextView
import android.graphics.Typeface
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.*
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.compose.LocalLifecycleOwner
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.activity.compose.rememberLauncherForActivityResult
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState); setContent {
            MaterialTheme(
                colorScheme = darkColorScheme(
                    primary = Color(0xff81d5b6),
                    secondary = Color(0xff9bbcff),
                    background = Color(0xff10151c),
                    surface = Color(0xff151e28)
                )
            ) { RemoteApp() }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun RemoteApp(vm: RemoteModel = viewModel()) {
    val state by vm.state.collectAsStateWithLifecycle()
    var showCommand by remember { mutableStateOf(false) }
    var tab by rememberSaveable { mutableIntStateOf(0) }
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val lifecycle = LocalLifecycleOwner.current.lifecycle
    DisposableEffect(vm, lifecycle) {
        val observer =
            LifecycleEventObserver { _, event ->
                if (event == Lifecycle.Event.ON_RESUME) vm.resumeVideoTracing()
                if (event == Lifecycle.Event.ON_PAUSE) vm.stopControls()
            }
        lifecycle.addObserver(observer)
        if (lifecycle.currentState.isAtLeast(Lifecycle.State.RESUMED)) vm.resumeVideoTracing()
        onDispose { lifecycle.removeObserver(observer) }
    }
    val permissions =
        rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            if (granted) vm.connect() else vm.log(
                "E",
                "Permission",
                "Bluetooth permission denied"
            )
        }
    val connect = {
        if (Build.VERSION.SDK_INT >= 31 && (state.settings.provider.isNotEmpty() || state.settings.mode != "WLAN") && state.settings.ble.isNotBlank() && context.checkSelfPermission(
                Manifest.permission.BLUETOOTH_CONNECT
            ) != android.content.pm.PackageManager.PERMISSION_GRANTED
        ) permissions.launch(Manifest.permission.BLUETOOTH_CONNECT) else vm.connect()
    }
    val export =
        rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
            if (uri != null) vm.export { text ->
                withContext(Dispatchers.IO) {
                    context.contentResolver.openOutputStream(uri, "wt")
                        ?.use { it.write(text.toByteArray()) }
                        ?: error("Unable to open destination")
                }
            }
        }
    val restore = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) scope.launch {
            runCatching {
                withContext(Dispatchers.IO) {
                    context.contentResolver.openInputStream(uri)?.use {
                        val bytes = java.io.ByteArrayOutputStream()
                        val buffer = ByteArray(4096)
                        var n = it.read(buffer); while (n >= 0) {
                        require(bytes.size() + n <= 262144) { "Backup exceeds 256 KiB" }; bytes.write(
                            buffer,
                            0,
                            n
                        ); n = it.read(buffer)
                    }; String(bytes.toByteArray(), Charsets.UTF_8)
                    } ?: error("Unable to open backup")
                }
            }.onSuccess { vm.restore(it) }
                .onFailure { vm.log("E", "Import", it.message ?: "Import failed") }
        }
    }
    val saveLog =
        rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("text/plain")) { uri ->
            if (uri != null) scope.launch(Dispatchers.IO) {
                runCatching {
                    context.contentResolver.openOutputStream(
                        uri,
                        "wt"
                    )
                        ?.use { out -> state.logs.forEach { out.write("${it.time} ${it.level}/${it.tag} ${it.text}\n".toByteArray()) } }
                }.onFailure { vm.log("E", "Export", it.message ?: "Log export failed") }
            }
        }
    if (state.projects.isEmpty()) {
        Onboarding(vm)
    } else if (state.connecting) {
        ConnectingScreen(state.stage, vm::cancelConnect)
    } else {
        val labels = listOf("Main", "Config", "Control", "Logs", "Settings")
        val icons = listOf(
            Icons.Outlined.Dashboard,
            Icons.Outlined.SmartToy,
            Icons.Outlined.Gamepad,
            Icons.Outlined.Terminal,
            Icons.Outlined.Settings
        )
        Scaffold(
            floatingActionButton = {
                FilledIconButton(
                    onClick = { showCommand = true },
                    enabled = state.connected
                ) { Icon(Icons.Outlined.Menu, "Remote commands") }
            },
            topBar = {
                TopAppBar(
                    title = {
                        Column {
                            ProjectMenu(state, vm); Text(
                            state.connection,
                            style = MaterialTheme.typography.labelSmall,
                            color = if (state.connected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant
                        )
                        }
                    },
                    actions = {
                        TextButton(onClick = { if (state.connected) vm.cancelConnect() else connect() }) {
                            Text(if (state.connected) "Disconnect" else "Connect")
                        }
                    })
            },
            bottomBar = {
                NavigationBar {
                    labels.forEachIndexed { i, label ->
                        NavigationBarItem(
                            selected = tab == i,
                            onClick = { vm.stopControls(); tab = i },
                            icon = { Icon(icons[i], label) },
                            label = { Text(label) })
                    }
                }
            }) { padding ->
            Box(Modifier
                .padding(padding)
                .fillMaxSize()) {
                when (tab) {
                    0 -> MainMenu(state, vm); 1 -> DeviceScreen(
                    state,
                    vm,
                    { export.launch("robot-config.json") },
                    {
                        restore.launch(
                            arrayOf(
                                "application/json",
                                "text/plain",
                                "application/octet-stream"
                            )
                        )
                    }); 2 -> ControlScreen(state, vm); 3 -> ConsoleScreen(
                    state,
                    vm,
                    { saveLog.launch("robot-log.txt") }); 4 -> SettingsScreen(state, vm)
                }
            }
        }
    }
    if (showCommand && state.connected) CommandDialog(state, vm) { showCommand = false }
    LaunchedEffect(state.connected) { if (!state.connected) showCommand = false }
    state.source?.let { text ->
        AlertDialog(
            onDismissRequest = vm::closeSource,
            title = { Text("RCP source reference") },
            text = {
                androidx.compose.foundation.text.selection.SelectionContainer {
                    Text(
                        text,
                        Modifier
                            .heightIn(max = 440.dp)
                            .verticalScroll(rememberScrollState())
                            .horizontalScroll(rememberScrollState()),
                        fontFamily = FontFamily.Monospace,
                        style = MaterialTheme.typography.bodySmall
                    )
                }
            },
            confirmButton = { TextButton(onClick = vm::closeSource) { Text("Close") } })
    }
    if (state.busy != null) LoadingDialog(state.busy!!, state.progress, state.total)
    if (state.error != null || state.notice != null) AlertDialog(
        onDismissRequest = vm::dismissError,
        title = { Text(if (state.error != null) "Check connection / operation" else "Done") },
        text = { Text(state.error ?: state.notice.orEmpty()) },
        confirmButton = { TextButton(onClick = vm::dismissError) { Text("OK") } })
}

@Composable
private fun ConnectingScreen(stage: String, cancel: () -> Unit) {
    Column(Modifier
        .fillMaxSize()
        .padding(32.dp), verticalArrangement = Arrangement.Center) {
        Text(
            "ROBOT LINK",
            style = MaterialTheme.typography.labelLarge,
            color = MaterialTheme.colorScheme.primary
        )
        Spacer(Modifier.height(16.dp)); Text(
        "Connecting",
        style = MaterialTheme.typography.headlineLarge
    )
        Spacer(Modifier.height(24.dp)); CircularProgressIndicator(); Spacer(Modifier.height(24.dp)); Text(
        stage
    )
        Spacer(Modifier.height(12.dp)); Text(
        "Transport → protobuf schema → config snapshot",
        style = MaterialTheme.typography.bodySmall
    )
        Spacer(Modifier.height(32.dp)); OutlinedButton(onClick = cancel) { Text("Cancel") }
    }
}

@Composable
private fun DeviceScreen(s: RemoteState, vm: RemoteModel, export: () -> Unit, restore: () -> Unit) {
    var search by rememberSaveable { mutableStateOf("") }
    var editing by remember { mutableStateOf<ConfigField?>(null) }
    var input by remember { mutableStateOf("") }
    var calibration by remember { mutableStateOf(false) }
    var colorCalibration by remember { mutableStateOf(false) }
    var colorSlot by rememberSaveable { mutableStateOf("0") }
    var validation by remember { mutableStateOf<String?>(null) }
    LaunchedEffect(s.connected) {
        if (!s.connected) {
            editing = null; calibration = false; colorCalibration = false
        }
    }
    val groups = s.config?.fields?.filter {
        it.name.contains(search, true) || it.group.contains(
            search,
            true
        ) || it.source.contains(search, true)
    }?.groupBy { it.group }.orEmpty()
    Column(Modifier.fillMaxSize()) {
        Row(
            Modifier
                .horizontalScroll(rememberScrollState())
                .padding(horizontal = 12.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            s.config?.fields.orEmpty().filter { it.sourceId > 0 }.distinctBy { it.sourceId }
                .forEach { field ->
                    TextButton(
                        onClick = { vm.source(field.sourceId) },
                        enabled = s.connected
                    ) { Text(field.source.substringAfterLast('/')) }
                }
            TextButton(onClick = vm::refresh, enabled = s.connected) { Text("Fetch") }; TextButton(
            onClick = export,
            enabled = s.connected
        ) { Text("Export") }; TextButton(
            onClick = restore,
            enabled = s.connected
        ) { Text("Restore") }; TextButton(
            onClick = { calibration = true },
            enabled = s.connected
        ) { Text("Calibrate gyro") }; TextButton(
            onClick = { colorCalibration = true },
            enabled = s.connected
        ) { Text("Legacy color calibration") }
        }
        OutlinedTextField(
            search,
            { search = it },
            label = { Text("Search config / source") },
            singleLine = true,
            modifier = Modifier
                .fillMaxWidth()
                .padding(12.dp)
        )
        if (groups.isEmpty()) Text(
            "Connect to fetch editable values from RCP.",
            Modifier.padding(20.dp)
        )
        LazyColumn(Modifier.weight(1f), contentPadding = PaddingValues(bottom = 16.dp)) {
            groups.forEach { (group, fields) ->
                item(key = "group:$group") {
                    Surface(
                        color = MaterialTheme.colorScheme.secondaryContainer,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text(
                            group,
                            Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                            style = MaterialTheme.typography.labelLarge
                        )
                    }
                }
                items(fields, key = { it.key }) { f ->
                    ListItem(
                        headlineContent = {
                            Text(
                                f.name,
                                fontFamily = FontFamily.Monospace
                            )
                        },
                        supportingContent = { Text("${f.source} · key ${f.key}", maxLines = 2) },
                        trailingContent = {
                            Text(
                                if (!s.connected) "—" else if (f.type == 2) (f.value != 0f).toString() else f.value.toString(),
                                fontFamily = FontFamily.Monospace
                            )
                        },
                        modifier = Modifier.clickable(enabled = s.connected) {
                            editing = f; input = f.value.toString(); validation = null
                        }); HorizontalDivider()
                }
            }
        }
    }
    editing?.let { f ->
        AlertDialog(
            onDismissRequest = { editing = null },
            title = { Text(f.name) },
            text = {
                Column {
                    Text(
                        "${f.minimum} … ${f.maximum}${if (f.type != 0) " · whole number" else ""}${
                            f.factoryDefault?.let { " · factory $it" }.orEmpty()
                        }"
                    ); if (f.type == 2) Row(verticalAlignment = Alignment.CenterVertically) {
                    Text("Enabled"); Switch(
                    checked = input.toFloatOrNull() != 0f,
                    onCheckedChange = { input = if (it) "1" else "0" })
                } else OutlinedTextField(
                    input,
                    { input = it },
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Decimal)
                ); validation?.let { Text(it, color = MaterialTheme.colorScheme.error) }
                }
            },
            confirmButton = {
                TextButton(
                    enabled = s.connected,
                    onClick = {
                        val v = input.toFloatOrNull()
                        val error = runCatching {
                            require(v != null); RobotSession.validate(
                            f,
                            v
                        )
                        }.exceptionOrNull(); if (error != null) validation =
                        error.message ?: "Invalid value" else {
                        vm.write(f, v!!); editing = null
                    }
                    }) { Text("Commit to flash") }
            },
            dismissButton = { TextButton(onClick = { editing = null }) { Text("Cancel") } })
    }
    if (calibration) AlertDialog(
        onDismissRequest = { calibration = false },
        title = { Text("Place robot still on a level surface") },
        text = { Text("Motors must be idle. Keep the robot still while 64 fresh gyro samples are checked. This saves gyro Z offset only; it does not calibrate the magnetometer or ToF.") },
        confirmButton = {
            TextButton(onClick = {
                calibration = false; vm.calibrate()
            }) { Text("Start calibration") }
        },
        dismissButton = { TextButton(onClick = { calibration = false }) { Text("Cancel") } })
    if (colorCalibration) AlertDialog(
        onDismissRequest = { colorCalibration = false },
        title = { Text("Calibrate legacy color slot") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Save normalized RGB for a legacy slot (0–3). These slots do not affect THS white-field navigation."); OutlinedTextField(
                colorSlot,
                { colorSlot = it.filter(Char::isDigit) },
                label = { Text("Color slot · 0–3") },
                singleLine = true,
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Number)
            ); validation?.let { Text(it, color = MaterialTheme.colorScheme.error) }
            }
        },
        confirmButton = {
            TextButton(onClick = {
                val slot = colorSlot.toIntOrNull(); if (slot == null || slot !in 0..3) validation =
                "Enter a slot from 0 to 3" else {
                colorCalibration = false; validation = null; vm.calibrateColor(slot)
            }
            }) { Text("Sample & save") }
        },
        dismissButton = {
            TextButton(onClick = {
                colorCalibration = false; validation = null
            }) { Text("Cancel") }
        })
}

// Log controls follow Vector: search, level/tag facets, follow-at-tail, wrapping, selectable rows.
// Android TextViews preserve native text selection for metadata/message groups.
@Composable
private fun ConsoleScreen(s: RemoteState, vm: RemoteModel, save: () -> Unit) {
    var query by rememberSaveable { mutableStateOf("") }
    var level by rememberSaveable { mutableStateOf("All") }
    var tag by rememberSaveable { mutableStateOf("") }
    var follow by rememberSaveable { mutableStateOf(true) }
    var wrap by rememberSaveable { mutableStateOf(true) }
    val rows = remember(
        s.logs,
        query,
        level,
        tag
    ) {
        s.logs.filter {
            (level == "All" || it.level == level) && (tag.isBlank() || it.tag.contains(
                tag,
                true
            )) && (it.text.contains(query, true) || it.tag.contains(query, true))
        }
    }
    val list = rememberLazyListState()
    LaunchedEffect(
        rows.lastOrNull()?.id,
        follow
    ) { if (follow && rows.isNotEmpty()) list.scrollToItem(rows.lastIndex) }
    LaunchedEffect(list.isScrollInProgress) {
        if (list.isScrollInProgress && list.layoutInfo.visibleItemsInfo.lastOrNull()?.index != rows.lastIndex) follow =
            false
    }
    Column(Modifier.fillMaxSize()) {
        IconButton(onClick = vm::logPolling) {
            Icon(
                if (s.logPolling) Icons.Outlined.Pause else Icons.Outlined.PlayArrow,
                "Toggle log updates"
            )
        }
        OutlinedTextField(
            query,
            { query = it },
            label = { Text("Search logs") },
            singleLine = true,
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 12.dp)
        )
        Row(
            Modifier
                .horizontalScroll(rememberScrollState())
                .padding(8.dp),
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            listOf("All", "D", "I", "W", "E").forEach { item ->
                FilterChip(
                    selected = level == item,
                    onClick = { level = item },
                    label = { Text(item) })
            }; FilterChip(
            selected = follow,
            onClick = { follow = !follow },
            label = { Text("Follow") }); FilterChip(
            selected = wrap,
            onClick = { wrap = !wrap },
            label = { Text("Wrap") }); TextButton(onClick = save) { Text("Save") }
        }
        OutlinedTextField(
            tag,
            { tag = it },
            label = { Text("Tag filter · EKF / Config / RPC") },
            singleLine = true,
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 12.dp)
        )
        Text(
            "${rows.size} / ${s.logs.size} entries · 2,000 entry buffer",
            Modifier.padding(12.dp),
            style = MaterialTheme.typography.labelSmall
        )
        LazyColumn(
            state = list,
            modifier = Modifier.weight(1f),
            contentPadding = PaddingValues(horizontal = 12.dp)
        ) {
            items(rows, key = { it.id }) { line ->
                AndroidView(
                    factory = { c ->
                        LinearLayout(c).apply {
                            orientation = LinearLayout.VERTICAL; setPadding(
                            8,
                            8,
                            8,
                            12
                        ); addView(TextView(c).apply {
                            typeface = Typeface.MONOSPACE; textSize = 10f; setTextIsSelectable(true)
                        }); addView(TextView(c).apply {
                            typeface = Typeface.MONOSPACE; textSize = 12f; setTextIsSelectable(true)
                        })
                        }
                    },
                    update = { group ->
                        val meta = group.getChildAt(0) as TextView
                        val body = group.getChildAt(1) as TextView
                        meta.text = "${line.time}   ${line.level}/${line.tag}"; meta.setTextColor(
                        when (line.level) {
                            "E" -> 0xffffb4ab.toInt(); "W" -> 0xffffd589.toInt(); else -> 0xff81d5b6.toInt()
                        }
                    ); body.text =
                        line.text; body.setTextColor(0xffdce4ed.toInt()); body.setHorizontallyScrolling(
                        !wrap
                    ); body.maxLines = if (wrap) Int.MAX_VALUE else 1
                    },
                    modifier = Modifier.fillMaxWidth()
                ); HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)
            }
        }

    }
}

@Composable
private fun SettingsScreen(s: RemoteState, vm: RemoteModel) {
    var ssid by remember { mutableStateOf("") }
    var password by remember { mutableStateOf("") }
    var about by remember { mutableStateOf(false) }
    var tools by rememberSaveable { mutableStateOf(false) }
    var videoTracing by rememberSaveable { mutableStateOf(false) }
    Column(
        Modifier
            .verticalScroll(rememberScrollState())
            .padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        ProjectSettings(s, vm)
        TransportProviderSettings(s, vm)
        Text("Connection", style = MaterialTheme.typography.headlineMedium)
        if (s.settings.provider.isEmpty()) {
            Row {
                listOf(
                    "Auto",
                    "WLAN",
                    "BLE"
                ).forEach { mode ->
                    FilterChip(
                        selected = s.settings.mode == mode,
                        onClick = { vm.settings(s.settings.copy(mode = mode)) },
                        label = { Text(mode) },
                        modifier = Modifier.padding(end = 8.dp)
                    )
                }
            }
            OutlinedTextField(
                s.settings.host,
                { vm.settings(s.settings.copy(host = it)) },
                label = { Text("Wi-Fi endpoint · discovered from BLE") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )
            OutlinedTextField(
                s.settings.pin,
                { vm.settings(s.settings.copy(pin = it)) },
                label = { Text("Certificate pin · learned from bonded BLE") },
                modifier = Modifier.fillMaxWidth()
            )
        }
        OutlinedTextField(
            s.settings.ble,
            { vm.settings(s.settings.copy(ble = it)) },
            label = { Text("BLE address · AA:BB:CC:DD:EE:FF") },
            singleLine = true,
            modifier = Modifier.fillMaxWidth()
        )
        if (s.settings.provider.isEmpty()) {
            Text(
                "Pair first: press EKF key 3 times within 3 seconds. BLE stays connected while Wi-Fi runs. The app discovers link-local + certificate over bonded BLE. Link loss never replays a command.",
                style = MaterialTheme.typography.bodySmall
            )
            HorizontalDivider(); Text(
            "Provision ESP WLAN via BLE",
            style = MaterialTheme.typography.titleMedium
        )
            OutlinedTextField(
                ssid,
                { ssid = it },
                label = { Text("SSID") },
                modifier = Modifier.fillMaxWidth()
            )
            OutlinedTextField(
                password,
                { password = it },
                label = { Text("Wi-Fi password") },
                visualTransformation = PasswordVisualTransformation(),
                modifier = Modifier.fillMaxWidth()
            )
            OutlinedButton(
                onClick = { vm.provision(ssid, password); password = "" },
                enabled = s.connected
            ) { Text("Send WLAN settings") }
        }
        HorizontalDivider()
        TextButton(onClick = { tools = !tools }, modifier = Modifier.fillMaxWidth()) {
            Text(
                "Tools",
                style = MaterialTheme.typography.titleMedium,
                modifier = Modifier.weight(1f)
            )
            Icon(
                if (tools) Icons.Outlined.ExpandLess else Icons.Outlined.ExpandMore,
                if (tools) "Collapse tools" else "Expand tools"
            )
        }
        if (tools) ListItem(
            headlineContent = { Text("Video tracing") },
            supportingContent = { Text(if (s.videoTracing.enabled) s.videoTracingStatus.message else "Start a recording when a robot log matches") },
            leadingContent = { Icon(Icons.Outlined.Videocam, null) },
            modifier = Modifier.clickable { videoTracing = true; vm.resumeVideoTracing() })
        HorizontalDivider(); Text(
        "Generic Field Tool · 0.1.0",
        style = MaterialTheme.typography.titleMedium
    ); Text("Open source · GPL-3.0-only")
        TextButton(onClick = { about = true }) { Text("Third-party notices & source references") }
    }
    if (videoTracing) VideoTracingDialog(s, vm) { videoTracing = false }
    if (about) {
        val c = LocalContext.current
        val notice = remember {
            runCatching {
                c.assets.open("THIRD_PARTY_NOTICES.md").bufferedReader().use { it.readText() }
            }.getOrDefault("See THIRD_PARTY_NOTICES.md in source distribution")
        }; AlertDialog(
            onDismissRequest = { about = false },
            title = { Text("Open-source notices") },
            text = {
                Text(
                    notice,
                    Modifier
                        .heightIn(max = 400.dp)
                        .verticalScroll(rememberScrollState())
                )
            },
            confirmButton = { TextButton(onClick = { about = false }) { Text("Close") } })
    }
}

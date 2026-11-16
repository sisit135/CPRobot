// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.RectangleShape
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Popup
import androidx.compose.ui.window.PopupProperties
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlin.math.roundToInt

@Composable
fun ControlScreen(s: RemoteState, vm: RemoteModel) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var failure by remember { mutableStateOf<String?>(null) }
    val buttons = s.projects.find { it.id == s.selectedProject }?.controls.orEmpty()
    val export = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        if (uri != null) scope.launch {
            runCatching {
                val json = vm.exportControls()
                withContext(Dispatchers.IO) { context.contentResolver.openOutputStream(uri, "wt")?.use { it.write(json.toByteArray()) }
                    ?: error("Cannot open destination") }
            }.onFailure { failure = it.message }
        }
    }
    val import = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) scope.launch {
            runCatching {
                val text = withContext(Dispatchers.IO) {
                    context.contentResolver.openInputStream(uri)?.use { stream ->
                        val out = java.io.ByteArrayOutputStream(); val buffer = ByteArray(4096)
                        while (true) {
                            val n = stream.read(buffer); if (n < 0) break
                            require(out.size() + n <= ControlLayout.MAX_BYTES) { "Layout exceeds 256 KiB" }
                            out.write(buffer, 0, n)
                        }; out.toString("UTF-8")
                    } ?: error("Cannot open layout")
                }; vm.importControls(text)
            }.onFailure { failure = it.message }
        }
    }
    DisposableEffect(vm, s.selectedProject) { onDispose { vm.stopControls() } }
    Column(Modifier.fillMaxSize()) {
        Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()).padding(horizontal = 12.dp),
            verticalAlignment = Alignment.CenterVertically) {
            FilterChip(selected = s.controlEditing, onClick = { vm.editControls(!s.controlEditing) }, label = { Text("Edit HUD") })
            TextButton(onClick = { export.launch("field-tool-controls.json") }) { Text("Export") }
            TextButton(onClick = { import.launch(arrayOf("application/json", "text/plain", "application/octet-stream")) }) { Text("Import") }
            Text(if (s.connected) "Connected" else "Offline", style = MaterialTheme.typography.labelSmall)
        }
        Text(if (s.controlEditing) "Long press empty space to add; long press a button to edit. Offsets use dp from screen edges."
            else "Tap to send; hold repeating controls to run. Enable Edit HUD to configure held controls.",
            Modifier.padding(horizontal = 16.dp, vertical = 4.dp), style = MaterialTheme.typography.bodySmall)
        BoxWithConstraints(Modifier.fillMaxWidth().weight(1f).background(MaterialTheme.colorScheme.background)) {
            val w = maxWidth.value; val h = maxHeight.value
            val density = LocalDensity.current.density
            val currentEdit by rememberUpdatedState(s.controlEditing)
            Box(Modifier.fillMaxSize().pointerInput(s.selectedProject, w, h) {
                detectTapGestures(onLongPress = { point ->
                    vm.editControl(ControlButton.at(point.x / density, point.y / density, w, h))
                })
            }) {
                if (buttons.isEmpty()) Text("Long press here to add a control", Modifier.align(Alignment.Center),
                    color = MaterialTheme.colorScheme.onSurfaceVariant)
                buttons.forEach { button -> key(button.id) {
                    val shape = when (button.shape) {
                        ControlShape.RECTANGLE -> RectangleShape
                        ControlShape.ROUNDED -> RoundedCornerShape(16.dp)
                        ControlShape.ELLIPSE -> CircleShape
                    }
                    val (x, y) = button.position(w, h)
                    val enabled = s.connected && !s.controlEditing
                    Box(Modifier.absoluteOffset { IntOffset((x * density).roundToInt(), (y * density).roundToInt()) }
                        .size(button.width.dp, button.height.dp).clip(shape)
                        .background(if (enabled) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant)
                        .border(if (s.controlEditing) 2.dp else 1.dp, MaterialTheme.colorScheme.outline, shape)
                        .pointerInput(button, s.connected) {
                            detectTapGestures(
                                onLongPress = {
                                    if (currentEdit || button.repeatMs == 0) {
                                        vm.releaseControl(button.id); vm.editControl(button)
                                    }
                                },
                                onTap = { if (!currentEdit && button.repeatMs == 0) vm.pressControl(button) },
                                onPress = {
                                    if (!currentEdit && button.repeatMs > 0) {
                                        vm.pressControl(button)
                                        try { tryAwaitRelease() } finally { vm.releaseControl(button.id) }
                                    }
                                })
                        }, contentAlignment = Alignment.Center) {
                        Column(horizontalAlignment = Alignment.CenterHorizontally) {
                            Text(button.label, Modifier.padding(4.dp), maxLines = 2)
                            if (s.controlEditing) Text("${button.horizontal.name.first()} ${button.horizontalOffset.toInt()} · ${button.vertical.name.first()} ${button.verticalOffset.toInt()}",
                                style = MaterialTheme.typography.labelSmall)
                        }
                    }
                } }
                if (s.controlEditing && buttons.any { val p = it.position(w, h); p.first < 0 || p.second < 0 || p.first + it.width > w || p.second + it.height > h }) {
                    TextButton(onClick = { vm.editControl(buttons.first { val p = it.position(w, h); p.first < 0 || p.second < 0 || p.first + it.width > w || p.second + it.height > h }) },
                        modifier = Modifier.align(Alignment.BottomCenter)) { Text("Edit an off-screen control") }
                }
            }
        }
    }
    s.controlEditor?.let { button -> key(button.id) { ControlEditor(button, s, vm) } }
    failure?.let { message -> AlertDialog(onDismissRequest = { failure = null }, title = { Text("Control layout") },
        text = { Text(message) }, confirmButton = { TextButton(onClick = { failure = null }) { Text("OK") } }) }
}

@Composable
private fun Choice(label: String, value: String, choices: List<String>, change: (String) -> Unit) {
    var menu by remember { mutableStateOf(false) }
    Box {
        TextButton(onClick = { menu = true }) { Text("$label: $value") }
        DropdownMenu(menu, { menu = false }) { choices.forEach {
            DropdownMenuItem(text = { Text(it) }, onClick = { change(it); menu = false })
        } }
    }
}

/** A focusable anchoring tooltip accepts an exact numeric offset rather than dragging coordinates. */
@Composable
private fun AnchorValue(label: String, value: String, change: (String) -> Unit) {
    var open by remember { mutableStateOf(false) }
    Box {
        OutlinedButton(onClick = { open = true }) { Text("$label + $value dp") }
        if (open) Popup(alignment = Alignment.TopCenter, onDismissRequest = { open = false },
            properties = PopupProperties(focusable = true)) {
            Surface(tonalElevation = 8.dp, shadowElevation = 8.dp, shape = RoundedCornerShape(12.dp)) {
                Column(Modifier.width(220.dp).padding(12.dp)) {
                    Text("Offset from $label")
                    OutlinedTextField(value, change, singleLine = true, label = { Text("Distance (dp)") })
                    TextButton(onClick = { open = false }) { Text("Done") }
                }
            }
        }
    }
}

@Composable
private fun ControlEditor(original: ControlButton, s: RemoteState, vm: RemoteModel) {
    val registry = remember(s.settings.provider, s.selectedProject, s.schemaTypes) { runCatching { vm.projectSchema() }.getOrNull() }
    val requests = registry?.names()?.filter { it.substringAfterLast('.').endsWith("Request") }?.sorted().orEmpty()
    var draft by remember { mutableStateOf(original.copy(request = original.request.ifEmpty { requests.firstOrNull().orEmpty() })) }
    var x by remember { mutableStateOf(draft.horizontalOffset.toString()) }
    var y by remember { mutableStateOf(draft.verticalOffset.toString()) }
    var width by remember { mutableStateOf(draft.width.toString()) }
    var height by remember { mutableStateOf(draft.height.toString()) }
    var repeat by remember { mutableStateOf(draft.repeatMs.toString()) }
    var release by remember { mutableStateOf(draft.releaseParameters.orEmpty()) }
    var failure by remember { mutableStateOf<String?>(null) }
    AlertDialog(onDismissRequest = { vm.editControl(null) }, title = { Text("Configure control") }, text = {
        Column(Modifier.heightIn(max = 500.dp).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedTextField(draft.label, { draft = draft.copy(label = it) }, label = { Text("Label") })
            Choice("Request", draft.request.ifEmpty { "Import a schema or select a provider" }, requests) {
                draft = draft.copy(request = it, parameters = "{}"); release = ""
            }
            val parameterObject = runCatching { org.json.JSONObject(draft.parameters) }.getOrNull()
            if (draft.request in requests && registry != null && parameterObject != null) key(draft.request) {
                ParameterFields(registry.descriptor(draft.request), parameterObject,
                    { draft = draft.copy(parameters = it.toString()) }, 0)
            }
            OutlinedTextField(draft.parameters, { draft = draft.copy(parameters = it) },
                label = { Text("Request parameters (JSON)") }, minLines = 2)
            Choice("Horizontal edge", draft.horizontal.name, HorizontalAnchor.entries.map { it.name }) { draft = draft.copy(horizontal = HorizontalAnchor.valueOf(it)) }
            AnchorValue(draft.horizontal.name.lowercase(), x) { x = it }
            Choice("Vertical edge", draft.vertical.name, VerticalAnchor.entries.map { it.name }) { draft = draft.copy(vertical = VerticalAnchor.valueOf(it)) }
            AnchorValue(draft.vertical.name.lowercase(), y) { y = it }
            Choice("Shape", draft.shape.name, ControlShape.entries.map { it.name }) { draft = draft.copy(shape = ControlShape.valueOf(it)) }
            OutlinedTextField(width, { width = it }, label = { Text("Width (dp)") }, singleLine = true)
            OutlinedTextField(height, { height = it }, label = { Text("Height (dp)") }, singleLine = true)
            OutlinedTextField(repeat, { repeat = it }, label = { Text("Hold repeat interval (ms; 0 = tap)") }, singleLine = true)
            OutlinedTextField(release, { release = it }, label = { Text("Release parameters (JSON; same request)") }, minLines = 2)
            failure?.let { Text(it, color = MaterialTheme.colorScheme.error) }
        }
    }, confirmButton = { TextButton(onClick = {
        runCatching { vm.saveControl(draft.copy(horizontalOffset = x.toFloat(), verticalOffset = y.toFloat(),
            width = width.toFloat(), height = height.toFloat(), repeatMs = repeat.toInt(), releaseParameters = release.takeIf { it.isNotBlank() })) }
            .onFailure { failure = it.message ?: "Invalid control" }
    }, enabled = requests.isNotEmpty()) { Text("Save") } }, dismissButton = {
        Row {
            TextButton(onClick = { vm.deleteControl(original.id) }) { Text("Delete") }
            TextButton(onClick = { vm.editControl(null) }) { Text("Cancel") }
        }
    })
}

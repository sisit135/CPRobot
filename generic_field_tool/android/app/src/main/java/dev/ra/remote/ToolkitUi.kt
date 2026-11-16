// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.google.protobuf.Descriptors
import org.json.JSONObject
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

@Composable
private fun SchemaPicker(vm: RemoteModel, content: @Composable ((String) -> Unit) -> Unit) {
    var project by rememberSaveable { mutableStateOf<String?>(null) }
    var failure by remember { mutableStateOf<String?>(null) }
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            val id = project; scope.launch {
                runCatching {
                    val bytes = withContext(Dispatchers.IO) {
                        context.contentResolver.openInputStream(uri)?.use { stream ->
                            val out = java.io.ByteArrayOutputStream()
                            val buffer = ByteArray(4096)
                            while (true) {
                                val n =
                                    stream.read(buffer); if (n < 0) break; require(out.size() + n <= 65536) { "Schema exceeds 64 KiB" }; out.write(
                                    buffer,
                                    0,
                                    n
                                )
                            }
                            out.toByteArray()
                        } ?: error("Cannot open schema")
                    }; if (id != null) vm.importSchema(id, bytes)
                }.onSuccess { failure = null; project = null }
                    .onFailure { failure = it.message ?: "Invalid schema" }
            }
        }
    }
    content { id -> project = id; picker.launch(arrayOf("*/*")) }
    if (failure != null) AlertDialog(
        onDismissRequest = { failure = null; project = null },
        title = { Text("Schema validation failed") },
        text = { Text("$failure\nSelect a binary FileDescriptorSet (.desc/.pb) generated with protoc --include_imports.") },
        confirmButton = {
            TextButton(onClick = {
                failure = null; picker.launch(arrayOf("*/*"))
            }) { Text("Pick another file") }
        },
        dismissButton = {
            TextButton(onClick = {
                failure = null; project = null
            }) { Text("Return to main") }
        })
}

@Composable
fun ProjectMenu(s: RemoteState, vm: RemoteModel) {
    var open by remember { mutableStateOf(false) }
    SchemaPicker(vm) { pick ->
        Box {
            TextButton(onClick = {
                open = true
            }) {
                Text(
                    s.projects.find { it.id == s.selectedProject }?.name ?: "Project"
                ); Icon(Icons.Outlined.ArrowDropDown, "Select project")
            }
            DropdownMenu(
                expanded = open,
                onDismissRequest = { open = false }) {
                s.projects.forEach { p ->
                    DropdownMenuItem(
                        text = { Text(p.name) },
                        onClick = { vm.selectProject(p.id); open = false },
                        trailingIcon = {
                            IconButton(onClick = {
                                open = false; pick(p.id)
                            }) { Icon(Icons.Outlined.Edit, "Import schema for ${p.name}") }
                        })
                }
            }
        }
    }
}

@Composable
fun ProjectSettings(s: RemoteState, vm: RemoteModel) {
    var editing by remember { mutableStateOf<ToolProject?>(null) }
    var adding by remember { mutableStateOf(false) }
    var name by remember { mutableStateOf("") }
    Text("Projects", style = MaterialTheme.typography.titleLarge)
    SchemaPicker(vm) { pick ->
        s.projects.forEach { p ->
            Row(verticalAlignment = Alignment.CenterVertically) {
                RadioButton(
                    selected = p.id == s.selectedProject,
                    onClick = { vm.selectProject(p.id) }); Text(
                p.name,
                Modifier
                 .weight(1f)
                 .clickable { vm.selectProject(p.id) }); IconButton(
                onClick = { pick(p.id) }) {
                Icon(
                    Icons.Outlined.Edit,
                    "Import schema for ${p.name}"
                )
            }; IconButton(onClick = { vm.removeProject(p.id) }) {
                Icon(
                    Icons.Outlined.Remove,
                    "Remove project"
                )
            }
            }
        }
    }
    Row {
        IconButton(onClick = { adding = true; name = "" }) {
            Icon(
                Icons.Outlined.Add,
                "Add project"
            )
        }; TextButton(onClick = {
        editing = s.projects.find { it.id == s.selectedProject }; name = editing?.name.orEmpty()
    }) { Text("Rename selected") }
    }
    if (adding || editing != null) AlertDialog(
        onDismissRequest = {
            adding = false; editing = null
        },
        title = { Text(if (adding) "New project" else "Edit project") },
        text = { OutlinedTextField(name, { name = it }, label = { Text("Name") }) },
        confirmButton = {
            TextButton(
                enabled = name.isNotBlank(),
                onClick = {
                    if (adding) vm.addProject(name) else editing?.let {
                        vm.renameProject(
                            it.id,
                            name
                        )
                    }; adding = false; editing = null
                }) { Text("Save") }
        },
        dismissButton = {
            TextButton(onClick = {
                adding = false; editing = null
            }) { Text("Cancel") }
        })
}

@Composable
fun MainMenu(s: RemoteState, vm: RemoteModel) {
    Column(
        Modifier
         .fillMaxSize()
         .verticalScroll(rememberScrollState())
         .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                "Sensors",
                style = MaterialTheme.typography.titleLarge,
                modifier = Modifier.weight(1f)
            ); IconButton(
            enabled = s.connected,
            onClick = vm::sensorPolling
        ) {
            Icon(
                if (s.sensorPolling) Icons.Outlined.Pause else Icons.Outlined.PlayArrow,
                "Toggle sensor updates"
            )
        }
        }
        val pose = if (s.connected) runCatching { JSONObject(s.pose) }.getOrNull() else null
        if (s.sensors.isEmpty()) Text(if (s.connected) "Waiting for generic sensor samples" else "—")
        s.sensors.forEach { (name, value) ->
            ListItem(
                headlineContent = { Text(name) },
                supportingContent = { Text(if (s.connected) value else "—") })
        }
        if (pose != null) {
            Text("Localization"); pose.keys().forEach { key ->
                ListItem(
                    headlineContent = { Text(key) },
                    trailingContent = { Text(pose.optString(key, "—")) })
            }
        }
        HorizontalDivider(); Text("Events", style = MaterialTheme.typography.titleLarge)
        if (s.events.isEmpty()) Text("Command responses will appear here")
        s.events.asReversed().forEach { e ->
            ListItem(
                headlineContent = { Text(e.tag) },
                supportingContent = { Text(e.text) },
                overlineContent = { Text(e.time) })
        }
    }
}

@Composable
fun CommandDialog(s: RemoteState, vm: RemoteModel, close: () -> Unit) {
    val registry = remember(s.selectedProject, s.schemaTypes) { vm.projectSchema() }
    val commands =
        registry?.names()?.filter { it.substringAfterLast('.').let { name -> name.endsWith("Command") || name.endsWith("Request") } }?.sorted()
            .orEmpty()
    var selected by remember { mutableStateOf(commands.firstOrNull().orEmpty()) }
    var menu by remember { mutableStateOf(false) }
    var json by remember(selected) { mutableStateOf(JSONObject()) }
    var error by remember { mutableStateOf<String?>(null) }
    AlertDialog(
        onDismissRequest = close,
        title = { Text("Remote command") },
        text = {
            Column(Modifier
             .heightIn(max = 440.dp)
             .verticalScroll(rememberScrollState())) {
                Box {
                    TextButton(onClick = {
                        menu = true
                    }) {
                        Text(selected.ifEmpty { "No *Request / *Command types in schema" }); Icon(
                        Icons.Outlined.ArrowDropDown,
                        null
                    )
                    }; DropdownMenu(
                    expanded = menu,
                    onDismissRequest = { menu = false }) {
                    commands.forEach { type ->
                        DropdownMenuItem(
                            text = { Text(type) },
                            onClick = { selected = type; menu = false; error = null })
                    }
                }
                }
                if (selected.isNotEmpty() && registry != null) key(selected) {
                    ParameterFields(
                        registry.descriptor(
                            selected
                        ), json, { json = it }, 0
                    )
                }
                Text(
                    "Matching *Response excerpts appear in Events. Maximum envelope: 238 bytes.",
                    style = MaterialTheme.typography.bodySmall
                )
                error?.let { Text(it, color = MaterialTheme.colorScheme.error) }
            }
        },
        confirmButton = {
            TextButton(
                enabled = s.connected && selected.isNotEmpty(),
                onClick = {
                    runCatching {
                        registry!!.json(
                            selected,
                            json.toString()
                        )
                    }.onSuccess { vm.sendCommand(selected, json.toString()); close() }
                        .onFailure { error = it.message }
                }) { Text("Send") }
        },
        dismissButton = { TextButton(onClick = close) { Text("Cancel") } })
}

@Composable
internal fun ParameterFields(
    descriptor: Descriptors.Descriptor,
    json: JSONObject,
    change: (JSONObject) -> Unit,
    depth: Int
) {
    if (depth > 8) {
        Text("Nesting exceeds form limit"); return
    }
    descriptor.fields.forEach { field ->
        key(field.number) {
            var expanded by remember { mutableStateOf(false) }
            var input by remember(json.toString()) { mutableStateOf(
                if (json.has(field.jsonName)) json.get(field.jsonName).toString() else ""
            ) }
            var invalid by remember { mutableStateOf(false) }
            TextButton(onClick = {
                expanded = !expanded
            }) {
                Icon(
                    if (expanded) Icons.Outlined.ExpandLess else Icons.Outlined.ExpandMore,
                    null
                ); Text("${field.name} · ${field.type}${if (field.isRepeated) " []" else ""}")
            }
            if (expanded) {
                if (field.javaType == Descriptors.FieldDescriptor.JavaType.MESSAGE && !field.isRepeated) {
                    val child = json.optJSONObject(field.jsonName) ?: JSONObject()
                    Column(Modifier.padding(start = 12.dp)) {
                        ParameterFields(
                            field.messageType,
                            child,
                            { value ->
                                change(
                                    JSONObject(json.toString()).put(
                                        field.jsonName,
                                        value
                                    )
                                )
                            },
                            depth + 1
                        )
                    }
                } else OutlinedTextField(
                    input,
                    { value ->
                        input = value
                        val next = JSONObject(json.toString()); runCatching {
                        if (value.isBlank()) next.remove(field.jsonName)
                        else next.put(
                            field.jsonName, when {
                                field.isRepeated -> org.json.JSONArray(value)
                                field.javaType == Descriptors.FieldDescriptor.JavaType.BOOLEAN -> {
                                    require(value == "true" || value == "false"); value.toBoolean()
                                }

                                field.javaType == Descriptors.FieldDescriptor.JavaType.STRING || field.javaType == Descriptors.FieldDescriptor.JavaType.BYTE_STRING || field.javaType == Descriptors.FieldDescriptor.JavaType.ENUM -> value
                                else -> org.json.JSONTokener(value).nextValue()
                                    .also { require(it is Number) }
                            }); change(next)
                    }.onSuccess { invalid = false }.onFailure {
                        invalid = true; change(
                        JSONObject(json.toString()).put(
                            field.jsonName,
                            value
                        )
                    )
                    }
                    },
                    isError = invalid,
                    label = { Text(if (field.isRepeated) "JSON array" else field.name) },
                    modifier = Modifier.fillMaxWidth()
                )
            }
        }
    }
}

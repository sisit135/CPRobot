// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier

@Composable
fun TransportProviderSettings(s: RemoteState, vm: RemoteModel) {
    var importing by rememberSaveable { mutableStateOf(false) }
    var entry by rememberSaveable { mutableStateOf("") }
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) vm.importProvider(uri, entry)
    }
    Text("Transport provider", style = MaterialTheme.typography.titleLarge)
    Row(verticalAlignment = Alignment.CenterVertically) {
        RadioButton(s.settings.provider.isEmpty(), onClick = { vm.settings(s.settings.copy(provider = "")) })
        Text("Built-in ESP32-C6 · BLE / WLAN")
    }
    s.providers.forEach { spec ->
        Row(verticalAlignment = Alignment.CenterVertically) {
            RadioButton(s.settings.provider == spec.key, onClick = { vm.settings(s.settings.copy(provider = spec.key)) })
            Column { Text(spec.label); Text("${spec.id} · ${spec.hash.take(12)}", style = MaterialTheme.typography.labelSmall) }
        }
    }
    TextButton(onClick = { importing = true }) { Text("Import provider DEX / ZIP / JAR") }
    if (s.settings.provider.isNotEmpty()) OutlinedTextField(s.settings.providerOptions,
        { vm.settings(s.settings.copy(providerOptions = it)) }, label = { Text("Provider options (JSON string values)") },
        modifier = Modifier.fillMaxWidth())
    if (importing) AlertDialog(onDismissRequest = { importing = false }, title = { Text("Import transport code") },
        text = { Column {
            Text("Provider files run with this app's permissions. Select a provider you trust. The file is copied to private storage.")
            OutlinedTextField(entry, { entry = it }, label = { Text("Entry class") })
        } }, confirmButton = { TextButton(onClick = { importing = false; picker.launch(arrayOf("*/*")) }, enabled = entry.isNotBlank()) { Text("Select file") } },
        dismissButton = { TextButton(onClick = { importing = false }) { Text("Cancel") } })
}

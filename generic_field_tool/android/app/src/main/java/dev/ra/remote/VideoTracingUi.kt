// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.provider.Settings
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp

@Composable
fun VideoTracingDialog(s: RemoteState, vm: RemoteModel, close: () -> Unit) {
    var enabled by rememberSaveable { mutableStateOf(s.videoTracing.enabled) }
    var pattern by rememberSaveable { mutableStateOf(s.videoTracing.pattern) }
    var modeName by rememberSaveable { mutableStateOf(s.videoTracing.mode.name) }
    var error by rememberSaveable { mutableStateOf<String?>(null) }
    val context = LocalContext.current
    val settings = VideoTracingSettings(enabled, pattern, VideoTracePatternMode.valueOf(modeName))
    val validation = remember(
        enabled,
        pattern,
        modeName
    ) { if (enabled) runCatching { VideoTracePattern(settings) }.exceptionOrNull()?.message else null }
    val permission =
        rememberLauncherForActivityResult(ActivityResultContracts.RequestPermission()) { granted ->
            if (granted) {
                if (vm.videoTracing(settings)) close()
            } else error = "Allow Nearby devices permission to use the Bluetooth shutter."
        }
    AlertDialog(onDismissRequest = close, title = { Text("Video tracing") }, text = {
        Column(
            Modifier
                .heightIn(max = 560.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Checkbox(
                    checked = enabled,
                    onCheckedChange = { enabled = it; error = null },
                    enabled = Build.VERSION.SDK_INT >= 28
                )
                Text("Use Video Tracing")
            }
            if (Build.VERSION.SDK_INT < 28) Text(
                "Bluetooth HID shutter needs Android 9 or newer.",
                color = MaterialTheme.colorScheme.error
            )
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                FilterChip(
                    selected = modeName == "GLOB",
                    onClick = { modeName = "GLOB"; error = null },
                    label = { Text("Glob") })
                FilterChip(
                    selected = modeName == "REGEX",
                    onClick = { modeName = "REGEX"; error = null },
                    label = { Text("Regex") })
            }
            OutlinedTextField(
                value = pattern,
                onValueChange = { pattern = it; error = null },
                label = { Text("Log keyword / pattern") },
                supportingText = { Text(if (modeName == "GLOB") "Whole message: *keyword* · * matches any text, ? matches one character" else "Search within message, for example: navigation.*started|run started") },
                isError = validation != null,
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )
            (error ?: validation)?.let { Text(it, color = MaterialTheme.colorScheme.error) }
            Text(
                "A new matching robot log attempts one volume-up shutter press, then disarms. No stop command is sent. Repeated logs and app restarts cannot toggle recording again until you rearm.",
                style = MaterialTheme.typography.bodySmall
            )
            Text(
                "On the recording phone, open the camera in video mode and configure its volume key to start recording. Camera state cannot be read back; a sent shutter is not confirmation that recording started.",
                style = MaterialTheme.typography.bodySmall
            )
            HorizontalDivider()
            Text(s.videoTracingStatus.message, style = MaterialTheme.typography.bodyMedium)
            if (s.videoTracing.enabled) {
                Text(
                    if (s.videoTracingStatus.armed) "Armed for one matching log" else "Disarmed",
                    style = MaterialTheme.typography.labelLarge
                )
                OutlinedButton(
                    onClick = vm::armVideoTracing,
                    enabled = !s.videoTracingStatus.armed && s.videoTracingStatus.connected
                ) { Text("Arm next recording") }
                s.videoTracingStatus.lastTrigger?.let {
                    Text(
                        it,
                        style = MaterialTheme.typography.bodySmall
                    )
                }
            }
            Text(
                "Save with tracing enabled, then pair the phones in system Bluetooth settings. Select Field Tool Shutter (or this phone's Bluetooth name) on the recording phone. Keep this app running to receive robot logs.",
                style = MaterialTheme.typography.bodySmall
            )
            TextButton(onClick = {
                runCatching { context.startActivity(Intent(Settings.ACTION_BLUETOOTH_SETTINGS)) }
                    .onFailure { error = "Bluetooth settings could not be opened on this device." }
            }) { Text("Open Bluetooth settings", textDecoration = TextDecoration.Underline) }
            if (s.videoTracing.enabled && s.videoTracingStatus.hosts.isNotEmpty()) {
                Text(
                    "Connect an already paired recording phone",
                    style = MaterialTheme.typography.labelLarge
                )
                s.videoTracingStatus.hosts.forEach { phone ->
                    TextButton(
                        onClick = { vm.connectVideoPhone(phone.address) },
                        enabled = !s.videoTracingStatus.connected
                    ) {
                        Text("${phone.name} · ${phone.address}")
                    }
                }
            }
        }
    }, confirmButton = {
        TextButton(enabled = validation == null, onClick = {
            if (enabled && Build.VERSION.SDK_INT >= 31 && context.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED)
                permission.launch(Manifest.permission.BLUETOOTH_CONNECT)
            else if (vm.videoTracing(settings)) close()
        }) { Text("Save") }
    }, dismissButton = { TextButton(onClick = close) { Text("Cancel") } })
}

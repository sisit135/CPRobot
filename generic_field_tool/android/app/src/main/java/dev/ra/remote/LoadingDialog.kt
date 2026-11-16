// SPDX-License-Identifier: GPL-3.0-only
// Adapted from kyujin-cho/pixel-volte-patch components/LoadingDialog.kt (GPLv3).
// Changes: title/progress parameters, removed previews and app-specific string resources,
// added progress bounds and kept the modal busy interaction.
package dev.ra.remote

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties

@Composable
fun LoadingDialog(title: String, current: Int, total: Int) {
    Dialog(
        onDismissRequest = {},
        properties = DialogProperties(dismissOnBackPress = false, dismissOnClickOutside = false)
    ) {
        Card(
            colors = CardDefaults.cardColors(
                containerColor = AlertDialogDefaults.containerColor,
                contentColor = AlertDialogDefaults.textContentColor
            ), modifier = Modifier.fillMaxWidth()
        ) {
            if (total > 0) Column(Modifier.padding(24.dp)) {
                Text(title, fontWeight = FontWeight.Bold)
                LinearProgressIndicator(
                    progress = { (current.toFloat() / total).coerceIn(0f, 1f) },
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(top = 24.dp, bottom = 8.dp)
                )
                Text("$current / $total")
            } else Row(
                Modifier.padding(24.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                CircularProgressIndicator(Modifier.size(32.dp)); Text(
                title,
                Modifier.padding(start = 16.dp)
            )
            }
        }
    }
}

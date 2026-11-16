// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2015 The Android Open Source Project
// Local Kotlin adaptation of the used Glif header/content/footer view structure.
// See app/SETUP_UI_SOURCES.md for original revisions and modifications.
package dev.ra.remote

import android.content.Context
import android.util.AttributeSet
import android.view.LayoutInflater
import android.view.View
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout

/** Only the view containers used by Field Tool; no partner config or OS setup services. */
class SetupWizardLayout @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null
) : LinearLayout(context, attrs) {
    val content: FrameLayout
    val primaryButton: Button
    val secondaryButton: Button

    init {
        orientation = VERTICAL
        LayoutInflater.from(context).inflate(R.layout.setup_wizard_frame, this, true)
        content = findViewById(R.id.setup_content)
        primaryButton = findViewById(R.id.setup_next_button)
        secondaryButton = findViewById(R.id.setup_back_button)
    }

    fun showHeader(visible: Boolean) {
        findViewById<View>(R.id.setup_header).visibility = if (visible) VISIBLE else GONE
    }
}

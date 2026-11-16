// SPDX-License-Identifier: MIT
// Copyright 2023-2025 GrapheneOS
// Adapted welcome/content views from SetupWizard2; see app/SETUP_UI_SOURCES.md.
// Local Kotlin MVVM, native footer controls and Compose paging replace library mixins.
package dev.ra.remote

import android.content.Context
import android.view.ContextThemeWrapper
import android.view.LayoutInflater
import android.view.View
import android.view.inputmethod.EditorInfo
import android.widget.EditText
import android.widget.ImageView
import android.widget.TextView
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.widget.doAfterTextChanged
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewmodel.compose.viewModel
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.filterNotNull

@Composable
fun Onboarding(vm: RemoteModel, wizard: OnboardingViewModel = viewModel()) {
    val state by wizard.state.collectAsStateWithLifecycle()
    val pager = rememberPagerState(initialPage = state.page, pageCount = { 3 })
    var programmaticScroll by remember { mutableStateOf(false) }
    val configuration = LocalConfiguration.current
    val context = LocalContext.current
    val background = remember(context, configuration) {
        context.resources.getColor(R.color.setup_background, context.theme)
    }
    LaunchedEffect(Unit) { wizard.begin() }
    // Only completed user swipes update the model; programmatic animation must
    // not feed its old settled page back into the requested page.
    LaunchedEffect(pager) {
        snapshotFlow {
            if (!pager.isScrollInProgress && !programmaticScroll) pager.settledPage else null
        }.filterNotNull().distinctUntilChanged().collect { wizard.selectPage(it) }
    }
    LaunchedEffect(state.page) {
        if (pager.currentPage != state.page) {
            programmaticScroll = true
            try {
                pager.animateScrollToPage(state.page)
            } finally {
                programmaticScroll = false
            }
        }
    }
    BackHandler(enabled = pager.currentPage > 0) {
        wizard.selectPage(pager.currentPage - 1)
    }
    HorizontalPager(
        state = pager, modifier = Modifier
            .fillMaxSize()
            .background(Color(background))
            .safeDrawingPadding()
            .imePadding()
    ) { page ->
        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            AndroidView(
                modifier = Modifier
                    .fillMaxHeight()
                    .widthIn(max = 720.dp)
                    .fillMaxWidth(),
                factory = { createWizardPage(it, page) },
                update = { root ->
                    bindWizardPage(
                        root, page, state.projectName,
                        wizard::changeName,
                        next = { wizard.selectPage(page); wizard.next(vm::addProject) },
                        back = { wizard.selectPage(page); wizard.back() })
                })
        }
    }
}

internal fun createWizardPage(base: Context, page: Int): SetupWizardLayout {
    require(page in 0..2)
    val context = ContextThemeWrapper(base, R.style.Theme_FieldTool_SetupWizard)
    val root = SetupWizardLayout(context)
    LayoutInflater.from(context).inflate(
        if (page == 0) R.layout.setup_welcome else R.layout.setup_step, root.content, true
    )
    root.primaryButton.setText(if (page == 2) R.string.setup_start else R.string.setup_next)
    root.secondaryButton.visibility = if (page > 0) View.VISIBLE else View.GONE
    root.showHeader(page > 0)
    if (page > 0) {
        root.findViewById<TextView>(R.id.setup_title).setText(
            if (page == 1) R.string.setup_connection_title else R.string.setup_project_title
        )
        root.findViewById<TextView>(R.id.setup_description).setText(
            if (page == 1) R.string.setup_connection_description else R.string.setup_project_description
        )
        root.findViewById<ImageView>(R.id.setup_icon).setImageResource(
            if (page == 2) R.drawable.baseline_done_all else R.drawable.setup_toolkit_icon
        )
        root.findViewById<View>(R.id.setup_body).visibility =
            if (page == 1) View.VISIBLE else View.GONE
        root.findViewById<View>(R.id.setup_project_container).visibility =
            if (page == 2) View.VISIBLE else View.GONE
    }
    val input = root.findViewById<EditText>(R.id.setup_project_name)
    val binding = WizardBinding(input)
    root.tag = binding
    input?.apply {
        doAfterTextChanged { if (!binding.rebinding) binding.changeName(it?.toString().orEmpty()) }
        setOnEditorActionListener { _, action, _ ->
            if (action == EditorInfo.IME_ACTION_DONE && text.isNotBlank()) {
                binding.next(); true
            } else false
        }
    }
    return root
}

internal fun bindWizardPage(
    root: SetupWizardLayout, page: Int, name: String,
    changeName: (String) -> Unit, next: () -> Unit, back: () -> Unit
) {
    val binding = root.tag as WizardBinding
    binding.changeName = changeName
    binding.next = next
    root.primaryButton.isEnabled = page < 2 || name.isNotBlank()
    root.primaryButton.setOnClickListener { next() }
    root.secondaryButton.setOnClickListener { back() }
    binding.input?.let { input ->
        if (input.text.toString() != name) {
            binding.rebinding = true
            try {
                input.setText(name); input.setSelection(name.length)
            } finally {
                binding.rebinding = false
            }
        }
    }
}

private class WizardBinding(val input: EditText?) {
    var rebinding = false
    var changeName: (String) -> Unit = {}
    var next: () -> Unit = {}
}

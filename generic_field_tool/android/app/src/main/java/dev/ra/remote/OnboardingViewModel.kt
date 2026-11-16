// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import androidx.lifecycle.SavedStateHandle
import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

data class OnboardingState(val page: Int = 0, val projectName: String = "") {
    val canContinue: Boolean get() = page < 2 || projectName.isNotBlank()
}

/** Owns wizard progress and the project draft across rotation/process recreation. */
class OnboardingViewModel(private val saved: SavedStateHandle) : ViewModel() {
    private val mutableState = MutableStateFlow(
        OnboardingState(
            (saved.get<Int>("setup.page") ?: 0).coerceIn(0, 2),
            saved.get<String>("setup.name").orEmpty()
        )
    )
    val state = mutableState.asStateFlow()
    private var completed = false

    fun selectPage(page: Int) {
        if (page !in 0..2) return
        update(mutableState.value.copy(page = page))
    }

    fun changeName(name: String) = update(mutableState.value.copy(projectName = name))

    fun back() = selectPage((mutableState.value.page - 1).coerceAtLeast(0))

    fun next(createProject: (String) -> Unit) {
        val current = mutableState.value
        if (completed || !current.canContinue) return
        if (current.page < 2) selectPage(current.page + 1)
        else {
            // Creation/persistence stays in RemoteModel; views only dispatch actions.
            createProject(current.projectName.trim())
            completed = true
            update(OnboardingState())
        }
    }

    /** A later first-project flow (after removing all projects) starts fresh. */
    fun begin() {
        if (completed) {
            completed = false
            update(OnboardingState())
        }
    }

    private fun update(next: OnboardingState) {
        saved["setup.page"] = next.page
        saved["setup.name"] = next.projectName
        mutableState.value = next
    }
}

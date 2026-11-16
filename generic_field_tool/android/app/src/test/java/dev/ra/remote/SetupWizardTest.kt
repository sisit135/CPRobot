// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import android.app.Activity
import android.widget.EditText
import androidx.lifecycle.SavedStateHandle
import kotlin.math.roundToInt
import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk=[34])
class SetupWizardTest {
    @Test fun welcomeUsesLocalFooterAndAdvances() {
        val controller=Robolectric.buildActivity(Activity::class.java).setup()
        try {
            val root=createWizardPage(controller.get(),0)
            var advanced=false
            bindWizardPage(root,0,"",{},{advanced=true},{})
            val footer=root
            assertEquals("Next",footer.primaryButton.text.toString())
            assertEquals(android.view.View.GONE,footer.secondaryButton.visibility)
            assertTrue(footer.primaryButton.performClick())
            assertTrue(advanced)
        } finally { controller.pause().stop().destroy() }
    }

    @Test fun projectDraftRequiresNameAndPreservesRestoredText() {
        val controller=Robolectric.buildActivity(Activity::class.java).setup()
        try {
            val root=createWizardPage(controller.get(),2)
            val footer=root
            var draft="";var submitted=false;var wentBack=false
            bindWizardPage(root,2,"",{draft=it},{submitted=true},{wentBack=true})
            assertFalse(footer.primaryButton.isEnabled)
            root.findViewById<EditText>(R.id.setup_project_name).setText("Bench target")
            assertEquals("Bench target",draft)
            bindWizardPage(root,2,draft,{draft=it},{submitted=true},{wentBack=true})
            assertTrue(footer.primaryButton.isEnabled)
            footer.secondaryButton.performClick();assertTrue(wentBack)
            footer.primaryButton.performClick();assertTrue(submitted)
            val restored=createWizardPage(controller.get(),2)
            bindWizardPage(restored,2,draft,{},{},{})
            assertEquals(draft,restored.findViewById<EditText>(R.id.setup_project_name).text.toString())
        } finally { controller.pause().stop().destroy() }
    }

    @Test @Config(qualifiers="sw700dp-w700dp-h960dp-night")
    fun nativeLayoutsInflateInNightModeWithTabletResources() {
        val controller=Robolectric.buildActivity(Activity::class.java).setup()
        try {
            val activity=controller.get()
            for(page in 0..2) {
                val root=createWizardPage(activity,page)
                assertNotNull(root.primaryButton)
                assertEquals((24*root.resources.displayMetrics.density).roundToInt(),
                    root.resources.getDimensionPixelSize(R.dimen.setup_content_padding_start))
            }
        } finally { controller.pause().stop().destroy() }
    }
    @Test fun modelRestoresDraftAndGuardsCompletion() {
        val saved = SavedStateHandle()
        val model = OnboardingViewModel(saved)
        var projects = emptyList<String>()
        val create: (String) -> Unit = { projects = projects + it }
        model.next(create)
        model.next(create)
        model.changeName("   ")
        model.next(create)
        assertTrue(projects.isEmpty())
        model.changeName("  Bench target  ")
        val restored = OnboardingViewModel(SavedStateHandle(mapOf(
            "setup.page" to saved.get<Int>("setup.page"),
            "setup.name" to saved.get<String>("setup.name"))))
        assertEquals(2, restored.state.value.page)
        assertEquals("  Bench target  ", restored.state.value.projectName)
        restored.next(create)
        restored.next(create)
        assertEquals(listOf("Bench target"), projects)
        restored.begin()
        assertEquals(OnboardingState(), restored.state.value)
    }
}

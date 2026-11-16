// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import org.junit.Assert.*
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config

@RunWith(RobolectricTestRunner::class)
@Config(sdk = [34])
class ProjectsTest {
    @Test fun projectRoundTripSeparatesLayoutsAndRestoresProviderSelection() {
        val prefs = RuntimeEnvironment.getApplication().getSharedPreferences("project-test", 0)
        prefs.edit().clear().commit()
        val a = ToolProject(id = "a", name = "A", settings = ConnectionSettings(provider = "hash-a", providerOptions = "{\"channel\":\"one\"}"),
            controls = listOf(ControlButton(id = "a-button", request = "sample.StartRequest", horizontal = HorizontalAnchor.RIGHT, horizontalOffset = 12f)))
        val b = ToolProject(id = "b", name = "B", controls = listOf(ControlButton(id = "b-button", request = "sample.StopRequest")))
        val store = ProjectStore(prefs)
        store.save(listOf(a, b), a.id)
        assertEquals(listOf(a, b), store.load())
        assertEquals("a", prefs.getString("selected", null))
    }
}

// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import com.google.protobuf.DescriptorProtos
import dev.ra.remote.proto.DynamicProto
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test

class ControlsTest {
    private fun schema(): DynamicProto {
        val file = DescriptorProtos.FileDescriptorProto.newBuilder().setName("controls.proto").setPackage("test").setSyntax("proto3")
        file.addMessageType(DescriptorProtos.DescriptorProto.newBuilder().setName("MoveRequest")
            .addField(DescriptorProtos.FieldDescriptorProto.newBuilder().setName("speed").setNumber(1).setType(DescriptorProtos.FieldDescriptorProto.Type.TYPE_INT32)))
        return DynamicProto(DescriptorProtos.FileDescriptorSet.newBuilder().addFile(file).build().toByteArray())
    }
    private fun button() = ControlButton(id = "go", label = "Go", request = "test.MoveRequest", parameters = "{\"speed\":42}",
        horizontal = HorizontalAnchor.RIGHT, horizontalOffset = 20f, vertical = VerticalAnchor.BOTTOM, verticalOffset = 30f,
        width = 100f, height = 80f, shape = ControlShape.ELLIPSE, repeatMs = 80, releaseParameters = "{\"speed\":0}")

    @Test fun anchorsKeepDistanceFromSelectedPhysicalEdgesAcrossResize() {
        val b = button()
        assertEquals(280f to 490f, b.position(400f, 600f))
        assertEquals(680f to 190f, b.position(800f, 300f))
        assertEquals(20f to 30f, b.copy(horizontal = HorizontalAnchor.LEFT, vertical = VerticalAnchor.TOP).position(800f, 300f))
    }
    @Test fun layoutRoundTripIncludesBindingReleaseShapeAndAnchorPairs() {
        assertEquals(listOf(button()), ControlLayout.decode(ControlLayout.encode(listOf(button())), schema()))
        val json = JSONObject(ControlLayout.encode(listOf(button())))
        val row = json.getJSONArray("controls").getJSONObject(0)
        assertFalse(row.has("x")); assertFalse(row.has("y"))
        assertEquals("RIGHT", row.getJSONObject("horizontal").getString("side"))
    }
    @Test fun importRejectsUnknownRequestsParametersAndDuplicatesBeforeReplacement() {
        val valid = ControlLayout.encode(listOf(button()))
        assertTrue(runCatching { ControlLayout.decode(valid.replace("MoveRequest", "MissingRequest"), schema()) }.isFailure)
        assertTrue(runCatching { ControlLayout.decode(valid.replace("speed", "unknown"), schema()) }.isFailure)
        assertTrue(runCatching { ControlLayout.decode(ControlLayout.encode(listOf(button(), button())), schema()) }.isFailure)
        assertTrue(runCatching { ControlLayout.decode(valid.replace("\"version\": 1", "\"version\": 2"), schema()) }.isFailure)
    }
    @Test fun invalidGeometryAndUnstoppableRepeatingControlAreRejected() {
        assertTrue(runCatching { button().copy(horizontalOffset = Float.NaN).validate() }.isFailure)
        assertTrue(runCatching { button().copy(width = -1f).validate() }.isFailure)
        assertTrue(runCatching { button().copy(releaseParameters = null).validate() }.isFailure)
        assertTrue(runCatching { button().copy(request = "test.MoveCommand").validate() }.isFailure)
    }
    @Test fun newButtonUsesNearestEdgesWithoutNormalizedCoordinates() {
        val b = ControlButton.at(380f, 580f, 400f, 600f)
        assertEquals(HorizontalAnchor.RIGHT, b.horizontal); assertEquals(VerticalAnchor.BOTTOM, b.vertical)
        assertEquals(0f, b.horizontalOffset, 0f); assertEquals(0f, b.verticalOffset, 0f)
    }
}

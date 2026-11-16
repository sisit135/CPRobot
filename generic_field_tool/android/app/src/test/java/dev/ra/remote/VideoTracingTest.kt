// SPDX-License-Identifier: GPL-3.0-only
package dev.ra.remote

import org.junit.Assert.*
import org.junit.Test

class VideoTracingTest {
 @Test fun globIsWholeMessageAndEscapesRegexPunctuation(){
  val matcher=VideoTracePattern(VideoTracingSettings(true,"*run [1].?*"))
  assertTrue(matcher.matches("navigation: run [1].x started"))
  assertFalse(matcher.matches("navigation: run 1Zx started"))
  assertFalse(VideoTracePattern(VideoTracingSettings(true,"started")).matches("run started"))
  assertTrue(VideoTracePattern(VideoTracingSettings(true,"*started*")).matches("run\nstarted"))
 }
 @Test fun regexFindsKeywordsAndRejectsInvalidExpressions(){
  val matcher=VideoTracePattern(VideoTracingSettings(true,"run (started|armed)",VideoTracePatternMode.REGEX))
  assertTrue(matcher.matches("navigation: run started at cell 2"))
  assertFalse(matcher.matches("run stopped"))
  assertTrue(runCatching{VideoTracePattern(VideoTracingSettings(true,"[",VideoTracePatternMode.REGEX))}.isFailure)
  assertTrue(runCatching{VideoTracePattern(VideoTracingSettings(true," "))}.isFailure)
 }
 @Test fun repeatedMatchingLogsCannotStopRecording(){
  val trigger=VideoTraceTrigger(VideoTracingSettings(true,"*start*"))
  var presses=0
  assertFalse(trigger.onLog("waiting"){presses++;true})
  repeat(20){trigger.onLog("start recording"){presses++;true}}
  assertEquals(1,presses)
  assertFalse(trigger.armed)
  trigger.arm()
  assertTrue(trigger.onLog("start recording"){presses++;true})
  assertEquals(2,presses)
 }
 @Test fun rejectedOrUnconfirmedSendRequiresExplicitRearming(){
  val trigger=VideoTraceTrigger(VideoTracingSettings(true,"*start*"))
  assertFalse(trigger.onLog("start"){false})
  assertFalse(trigger.armed)
  assertFalse(trigger.onLog("start"){fail("Must not retry an unconfirmed shutter");true})
 }
 @Test fun restoredDisarmedStateDoesNotReplayAfterRestart(){
  val settings=VideoTracingSettings(true,"*start*")
  val restored=VideoTraceTrigger(settings,initiallyArmed=false)
  assertFalse(restored.onLog("start"){fail("Restart must not rearm the camera");true})
  restored.arm()
  assertTrue(restored.onLog("start"){true})
 }
 @Test fun disabledTracingCannotStartArmed(){
  val trigger=VideoTraceTrigger(VideoTracingSettings(false,"*"),initiallyArmed=true)
  assertFalse(trigger.onLog("start"){fail("Disabled tracing must not send");true})
 }
}

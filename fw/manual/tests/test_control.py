# SPDX-License-Identifier: GPL-3.0-only
"""ADR drive, atomic manipulator and safety contracts; parked tests stay commented."""
import importlib.util
from pathlib import Path
import unittest
import math
import _thread
import contextlib
import io
import threading
import time
import sys
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('adr', ROOT / 'fw/manual/main.py')
adr = importlib.util.module_from_spec(spec)
spec.loader.exec_module(adr)


class Hardware:
    def __init__(self):
        self.power = (0, 0)
        self.servos = []
        self.angles = {}
        self.adds = []
        self.cranks = []
    def stop(self):
        self.power = (0, 0)
        self.cranks.append(0)
    def drive(self, left, right): self.power = (left, right)
    def servo(self, angle, port):
        self.servos.append((angle, port))
        self.angles[port] = angle
    def servo_add(self, delta, port):
        self.adds.append((delta, port))
        self.angles[port] += delta
        self.servos.append((self.angles[port], port))
    def crank(self, power): self.cranks.append(power)


class ManipulatorTest(unittest.TestCase):
    def setUp(self):
        self.hw = Hardware()
        self.dispatch = adr.Dispatcher(self.hw, lambda a, b: a - b)
        self.now = 1000
        self.step()
        self.step(arm=True)
        self.assertTrue(self.dispatch.armed)

    def step(self, dt=.02, **values):
        self.now += int(round(dt * 1000))
        r = adr.DriveRequest(self.now, connected=True, **values)
        self.dispatch.step(r, False, self.now, dt)

    def test_first_arm_establishes_configured_neutral_once(self):
        self.assertEqual([(adr.GRIP1_NEUTRAL_DEG, 's2'),
                          (adr.GRIP2_NEUTRAL_DEG, 's3'), (45, 's1')], self.hw.servos)
        self.step()
        self.assertEqual(3, len(self.hw.servos))
        self.assertEqual([], self.hw.adds)

    def test_flap_extends_outward_once_from_latest_pose_and_ignores_step(self):
        manip = self.dispatch.manipulators
        self.assertEqual('N4', adr.FLAP_KEY)
        manip.grip_left.command(150)
        manip.grip_right.command(30)
        self.hw.adds.clear()
        with patch.object(manip.guard, 'allows', side_effect=AssertionError('guard called')), \
             patch.object(manip.guard, 'allows_pair', side_effect=AssertionError('guard called'), create=True):
            self.step(flap=True)
            for _ in range(20):
                self.step(flap=True)
        # Hardware correction: the original 0/180 targets drove the beams inward.
        self.assertEqual([(30, 's2'), (-30, 's3')], self.hw.adds)
        self.assertEqual({'s2': 180, 's3': 0, 's1': 45}, self.hw.angles)
        self.step()
        self.step(flap=True)  # At the target: no zero-delta writes.
        self.assertEqual(2, len(self.hw.adds))
        self.step(grip_left=-1)
        self.step(flap=True)
        self.assertEqual([(-1, 's2'), (1, 's2')], self.hw.adds[-2:])


    def test_flap_clamps_targets_and_preflights_both_references(self):
        manip = self.dispatch.manipulators
        manip.grip_left.high, manip.grip_right.low = 170, 10
        self.step(flap=True)
        self.assertEqual([(80, 's2'), (-80, 's3')], self.hw.adds)
        self.step()
        manip.grip_left.command(90)
        manip.grip_right.command(90)
        manip.grip_right.initialized = False
        self.hw.adds.clear()
        self.step(flap=True)
        self.assertEqual([], self.hw.adds)
        self.assertEqual((90, 90), (manip.grip_left.position, manip.grip_right.position))


    def test_flap_write_failure_drops_remaining_commands_without_retry(self):
        for failed_port in ('s2', 's3'):
            with self.subTest(failed_port=failed_port):
                self.setUp()
                records, attempts = [], []
                manip = self.dispatch.manipulators
                manip.logger = SimpleNamespace(emit=lambda *args: records.append(args))
                write = self.hw.servo_add
                def failing(delta, port):
                    attempts.append((delta, port))
                    if port == failed_port:
                        raise OSError('write unavailable')
                    write(delta, port)
                with patch.object(self.hw, 'servo_add', side_effect=failing):
                    self.step(flap=True)
                    for _ in range(20):
                        self.step(flap=True)
                    for _ in range(20):
                        self.step()
                self.assertEqual([(90, 's2')] + ([(-90, 's3')] if failed_port == 's3' else []), attempts)
                self.assertEqual((90 if failed_port == 's2' else 180, 90),
                                 (manip.grip_left.position, manip.grip_right.position))
                self.assertEqual(('WARN', 'flap'), records[-1][:2])


    def test_flap_never_queues_while_disarmed_or_on_stop(self):
        for cause in ('estop', 'disconnect', 'stale'):
            with self.subTest(cause=cause):
                self.setUp()
                self.now += 20
                r = adr.DriveRequest(self.now, connected=cause != 'disconnect',
                                     estop=cause == 'estop', flap=True)
                self.dispatch.step(r, False, self.now + (201 if cause == 'stale' else 0), .02)
                self.assertFalse(self.dispatch.armed)
                self.step(flap=True)
                self.step(flap=True, arm=True)
                self.assertFalse(self.dispatch.armed)
                self.step()
                self.step(arm=True)
                self.assertTrue(self.dispatch.armed)
                self.assertEqual([], self.hw.adds)
                self.step(flap=True)
                self.assertEqual([(90, 's2'), (-90, 's3')], self.hw.adds)


    def test_flap_macro_and_manual_conflicts_consume_edges(self):
        for values in ({'shoot': True},
                       {'grip_left': 1}, {'grip_right': -1}, {'pitch': 1}):
            with self.subTest(values=values):
                self.setUp()
                self.step(flap=True, **values)
                self.assertEqual('IDLE', self.dispatch.launcher.state)
                expected = ([(1, 's2')] if 'grip_left' in values else
                            [(-1, 's3')] if 'grip_right' in values else
                            [(1, 's1')] if 'pitch' in values else [])
                self.assertEqual(expected, self.hw.adds)
                self.step(flap=True)
                self.assertEqual(expected, self.hw.adds)  # No delayed macro.
                self.step()
                self.hw.adds.clear()
                self.step(flap=True)
                self.assertEqual((180, 0), (self.hw.angles['s2'], self.hw.angles['s3']))
                self.assertEqual(['s2', 's3'], [port for _, port in self.hw.adds])


    def test_beams_move_independently_once_until_centered(self):
        manip = self.dispatch.manipulators
        for _ in range(10):
            self.step(grip_left=-1)
        self.assertEqual(adr.GRIP1_NEUTRAL_DEG - 1, manip.grip_left.position)
        self.assertEqual(adr.GRIP2_NEUTRAL_DEG, manip.grip_right.position)
        self.assertEqual([(-1, 's2')], self.hw.adds)
        self.step(grip_left=1)  # Reversal without centering is still the same gesture.
        self.assertEqual([(-1, 's2')], self.hw.adds)
        self.step()
        self.step(grip_right=1)
        self.assertEqual([(-1, 's2'), (1, 's3')], self.hw.adds)
        self.step(grip_left=-.01, grip_right=1)
        self.assertEqual((-1, 's2'), self.hw.adds[-1])
        self.assertEqual(3, len(self.hw.adds))

    def test_pitch_atomic_steps_clamp_robot_envelope_on_port_one(self):
        self.step(step_previous=True)  # 1 -> 60 degrees.
        self.step()
        for _ in range(3):
            self.step(pitch=-1)
            self.step()
        self.assertEqual(-45, self.dispatch.manipulators.pitch.position)
        self.assertEqual([(-45, 's1')], self.hw.adds)
        for _ in range(4):
            self.step(pitch=1)
            self.step()
        self.assertEqual(135, self.dispatch.manipulators.pitch.position)
        self.assertEqual([(-45, 's1')] + [(60, 's1')] * 3, self.hw.adds)
        self.assertTrue(all(0 <= a <= 180 for a, _ in self.hw.servos))

    def test_all_step_sizes_cycle_once_per_trigger_press_and_publish(self):
        published = []
        manip = self.dispatch.manipulators
        manip.logger = SimpleNamespace(set_servo_step=published.append, emit=lambda *args: None)
        self.assertEqual((1, 2, 5, 15, 30, 60), adr.SERVO_STEPS_DEG)
        for step in (2, 5, 15, 30, 60, 1):
            self.step(step_next=True)
            for _ in range(4):
                self.step(step_next=True)
            self.assertEqual(step, adr.SERVO_STEPS_DEG[manip.step_index])
            self.step()
        self.assertEqual([2, 5, 15, 30, 60, 1], published)
        self.step(step_previous=True, step_next=True)
        self.step(step_previous=True)
        self.assertEqual(0, manip.step_index)  # Both must release after conflicting keys.
        self.step()
        self.step(step_previous=True)
        self.assertEqual(60, adr.SERVO_STEPS_DEG[manip.step_index])

    def test_changing_step_while_axis_held_does_not_queue_motion(self):
        self.step(grip_left=-1)
        self.step(grip_left=-1, step_next=True)
        self.step(grip_left=-1)
        self.assertEqual([(-1, 's2')], self.hw.adds)
        self.step()
        self.step(grip_left=-1, dt=.1)
        self.assertEqual([(-1, 's2'), (-2, 's2')], self.hw.adds)

    def test_safety_gates_require_fresh_gesture_and_preserve_commanded_poses(self):
        for fault in ('disconnect', 'estop', 'stale', 'future', 'latched'):
            with self.subTest(fault=fault):
                hw = Hardware()
                d = adr.Dispatcher(hw, lambda a, b: a - b)
                d.armed = True
                d.step(adr.DriveRequest(1000, connected=True), False, 1000, .02)
                d.step(adr.DriveRequest(1020, connected=True, grip_left=-1,
                                       grip_right=1, pitch=1), False, 1020, .02)
                servos = d.manipulators.servos
                pose, writes = [v.position for v in servos], len(hw.servos)
                stamp = 839 if fault == 'stale' else 1041 if fault == 'future' else 1040
                r = adr.DriveRequest(stamp, connected=fault != 'disconnect',
                                     estop=fault == 'estop', grip_left=-1, pitch=1)
                d.step(r, fault == 'latched', 1040, .02)
                self.assertFalse(d.armed)
                self.assertTrue(all(not v.input_ready for v in servos))
                self.assertEqual((0, 0), hw.power)
                for now, arm in ((1060, False), (1080, True), (1100, False)):
                    d.step(adr.DriveRequest(now, connected=True, arm=arm), False, now, .02)
                self.assertTrue(d.armed)
                self.assertEqual(pose, [v.position for v in servos])
                self.assertEqual(writes, len(hw.servos))

    def test_manual_movement_blocks_arming(self):
        for values in ({'grip_left': 1}, {'grip_left': -1}, {'grip_right': 1},
                       {'grip_right': -1}, {'pitch': 1}, {'pitch': -1}):
            with self.subTest(values=values):
                hw = Hardware()
                d = adr.Dispatcher(hw, lambda a, b: a - b)
                d.step(adr.DriveRequest(1000, connected=True), False, 1000, .02)
                d.step(adr.DriveRequest(1020, connected=True, arm=True, **values), False, 1020, .02)
                self.assertFalse(d.armed)
                self.assertEqual([], hw.servos)

    def test_manual_beams_bypass_beamguard_but_still_clamp_and_require_fresh_input(self):
        records = []
        manip = self.dispatch.manipulators
        manip.grip_right.logger = SimpleNamespace(emit=lambda *args: records.append(args))
        self.step(step_previous=True)  # 60 degrees.
        self.step()
        with patch.object(manip.guard, 'allows', side_effect=AssertionError('guard called')):
            self.step(grip_left=1, grip_right=-1, pitch=1, left=10, right=20)
        self.assertEqual([(60, 's2'), (-60, 's3'), (60, 's1')], self.hw.adds)
        self.assertEqual((150, 30), (manip.grip_left.position, manip.grip_right.position))
        self.assertEqual((10, 20), self.hw.power)
        self.assertTrue(self.dispatch.armed)
        self.assertEqual([], records)
        for _ in range(10):
            self.step(grip_left=1, grip_right=-1, pitch=1)
        self.assertEqual([], records)
        self.assertEqual(3, len(self.hw.adds))
        self.step()
        self.step(grip_right=-1)
        self.assertEqual(0, manip.grip_right.position)
        self.assertEqual((-30, 's3'), self.hw.adds[-1])
        self.step()
        self.step(grip_right=-1)
        self.assertEqual(4, len(self.hw.adds))

#     def test_catapult_immediate_full_power_hold_reverse_and_release(self):
#         self.step(crank=1)
#         self.assertEqual(100, self.hw.cranks[-1])
#         for _ in range(60): self.step(crank=1)  # Hold beyond the former 700 ms limit.
#         self.assertEqual(100, self.hw.cranks[-1])
#         self.step(crank=-1)
#         self.assertEqual(-100, self.hw.cranks[-1])
#         self.step()
#         self.assertEqual(0, self.hw.cranks[-1])
#
#     def test_power_steps_once_per_press_clamps_and_applies_while_held(self):
#         self.step(crank=1, power_adjust=-1)
#         self.assertEqual(90, self.hw.cranks[-1])
#         for _ in range(10): self.step(crank=1, power_adjust=-1)
#         self.assertEqual(90, self.hw.cranks[-1])
#         for _ in range(12):
#             self.step(crank=1)
#             self.step(crank=1, power_adjust=-1)
#         self.assertEqual(0, self.hw.cranks[-1])
#         for _ in range(12):
#             self.step(crank=-1)
#             self.step(crank=-1, power_adjust=1)
#         self.assertEqual(-100, self.hw.cranks[-1])
#
#     def test_catapult_stops_on_safety_gates(self):
#         for reason in ('deadman', 'disconnect', 'estop', 'stale'):
#             with self.subTest(reason=reason):
#                 hw = Hardware()
#                 dispatcher = adr.Dispatcher(hw, lambda a, b: a - b)
#                 dispatcher.armed = True
#                 r = adr.DriveRequest(1000, connected=True, deadman=True, crank=1)
#                 dispatcher.step(r, False, 1000, .02)
#                 self.assertEqual(100, hw.cranks[-1])
#                 r.deadman = reason != 'deadman'
#                 r.connected = reason != 'disconnect'
#                 r.estop = reason == 'estop'
#                 dispatcher.step(r, False, 1201 if reason == 'stale' else 1020, .02)
#                 self.assertEqual(0, hw.cranks[-1])
#
    def test_failed_other_beam_command_does_not_change_clearance_reference(self):
        self.step(step_previous=True)
        self.step()
        add = self.hw.servo_add
        def fail_left(delta, port):
            if port == 's2':
                raise OSError('left write failed')
            add(delta, port)
        self.hw.servo_add = fail_left
        self.step(grip_left=1, grip_right=-1)
        manip = self.dispatch.manipulators
        self.assertEqual((90, 30), (manip.grip_left.position, manip.grip_right.position))
        self.assertEqual([(-60, 's3')], self.hw.adds)

    def test_beam_waits_for_other_issued_reference_then_requires_new_input(self):
        hw = Hardware()
        manip = adr.Manipulators(hw, lambda a, b: a - b)
        write = hw.servo
        def missing_right(angle, port):
            if port == 's3':
                raise OSError('reference write failed')
            write(angle, port)
        hw.servo = missing_right
        manip.update(0, 0, 0, 1000, .02)
        manip.update(-1, 0, 1, 1020, .02)
        self.assertEqual([(1, 's1')], hw.adds)
        hw.servo = write
        manip.update(-1, 1, 0, 1040, .02)  # Right user input establishes its reference.
        manip.update(-1, 0, 0, 1060, .02)
        self.assertEqual([(1, 's1')], hw.adds)
        manip.update(0, 0, 0, 1080, .02)
        manip.update(-1, 0, 0, 1100, .02)
        self.assertEqual((-1, 's2'), hw.adds[-1])

    def test_vendor_calls_and_conflicting_buttons(self):
        calls, keys, queried, axes = [], set(), [], dict.fromkeys(('Lx', 'Ly', 'Rx', 'Ry'), 0)
        mbot = SimpleNamespace(drive_speed=lambda left, right: calls.append(('wheel', left, right)),
            motor_set=lambda power, port: calls.append(('dc', power, port)),
            servo_set=lambda angle, port: calls.append(('servo', angle, port)),
            servo_add=lambda delta, port: calls.append(('add', delta, port)))
        pad = SimpleNamespace(is_online=lambda: True, is_key_pressed=lambda key: queried.append(key) or key in keys,
                              get_joystick=lambda name: axes[name])
        cpi = SimpleNamespace(mbot2=mbot, controller=SimpleNamespace(is_press=lambda key: False))
        hw = adr.CyberPiHardware(cpi, pad, mbot)
        hw.servo(30, adr.GRIP1_PORT)
        hw.drive(10, 20)
        hw.stop()
        self.assertIn(('servo', 30, 's2'), calls)
        self.assertIn(('wheel', 10, -20), calls)
        self.assertIn(('wheel', 0, 0), calls)
        self.assertFalse(hasattr(hw, 'crank'))
        self.assertTrue(all(call[1] == 0 for call in calls if call[0] == 'dc'))
        self.assertEqual(('dc', 0, 'm1'), calls[-1])
        limit = adr.drive_limit()
        hw.drive(1000, -1000)
        self.assertEqual(('wheel', limit, limit), calls[-1])
        # RWD: Ly controls only E1 (first argument); Ry only E2 (second).
        hw.drive(*adr.wheel_targets(100, 0, 0))
        self.assertEqual(('wheel', limit, 0), calls[-1])
        hw.drive(*adr.wheel_targets(0, 100, 0))
        self.assertEqual(('wheel', 0, -limit), calls[-1])
        with patch.object(adr.time, 'ticks_ms', lambda: 1000, create=True):
            keys.update(('L1', 'R1'))
            r = hw.sample()
            self.assertEqual((0, 0, 0, 0, None),
                             (r.crank, r.power_adjust, r.grip_left, r.grip_right, r.pitch))
            keys.remove('R1')
            axes['Lx'] = -100
            r = hw.sample()
            self.assertEqual((-1, 0, -1), (r.grip_left, r.grip_right, r.pitch))
            self.assertEqual((0, 0), (r.left, r.right))
            keys.clear()
            keys.add('R1')
            axes['Lx'], axes['Rx'] = 0, 100
            r = hw.sample()
            self.assertEqual((0, -1, 1), (r.grip_left, r.grip_right, r.pitch))
            axes['Rx'] = 50
            self.assertTrue(-1 < hw.sample().grip_right < 0)
            axes['Rx'] = 5
            self.assertEqual(0, hw.sample().grip_right)
            axes['Lx'] = None
            with self.assertRaises(ValueError):
                hw.sample()
            axes['Lx'] = 0
            keys.clear()
            keys.update(('N1', 'N2', 'N3', 'N4', 'L2', 'R2', 'L_Thumb', 'R_Thumb', 'Up', 'Down'))
            r = hw.sample()
            self.assertEqual((0, 0, None), (r.grip_left, r.grip_right, r.pitch))
            self.assertTrue(r.shoot and r.release_mode)
            self.assertTrue(r.flap)
            self.assertTrue(r.step_previous and r.step_next)
            self.assertTrue({'L1', 'R1', 'L2', 'R2', 'N1', 'N2', 'N4'}.issubset(queried))
            self.assertTrue(set(queried).isdisjoint({'N3', 'L_Thumb', 'R_Thumb', 'Up', 'Down'}))

    def test_native_pid_targets_are_bounded_without_host_slew(self):
        self.step(left=20, right=-20)
        self.assertEqual((20, -20), self.hw.power)
        self.step(left=-20, right=20)
        self.assertEqual((-20, 20), self.hw.power)
        self.step(left=100, right=-100)
        self.assertEqual((100, -100), self.hw.power)
        self.step(left=1000, right=-1000)
        self.assertEqual((adr.drive_limit(), -adr.drive_limit()), self.hw.power)
        self.step()
        self.assertEqual((0, 0), self.hw.power)

    def test_bypass_selects_power_api_and_uses_independent_power_limit(self):
        calls = []
        # No speed API: bypass mode must not require or call it, even to stop.
        mbot = SimpleNamespace(drive_power=lambda *args: calls.append(('power', args)),
                               motor_set=lambda *args: calls.append(('dc', args)),
                               servo_set=lambda *args: None, servo_add=lambda *args: None)
        with patch.object(adr, 'PID_BYPASS', True), patch.object(
                adr, 'MAX_DRIVE_POWER', 65), patch.object(adr, 'MAX_RPM', 27):
            hw = adr.CyberPiHardware(SimpleNamespace(mbot2=mbot), None, mbot)
            targets = adr.wheel_targets(100, -100, 0)
            self.assertEqual((65, -65), targets)
            self.step(left=200, right=-200)
            self.assertEqual(targets, self.hw.power)
            hw.drive(200, -200)
            self.assertEqual(('power', (65, 65)), calls[-1])
            hw.stop()
            self.assertEqual([('power', (0, 0)), ('dc', (0, 'm1'))], calls[-2:])
        with patch.object(adr, 'MAX_DRIVE_POWER', 65), patch.object(adr, 'MAX_RPM', 27):
            self.assertEqual((27, -27), adr.wheel_targets(100, -100, 0))

    def test_disabled_catapult_requests_cannot_actuate_or_block_arming(self):
        self.assertFalse(hasattr(adr, 'Cranker'))
        for direction in (-1, 1):
            self.step(crank=direction, power_adjust=direction)
        self.assertFalse(any(self.hw.cranks))
        dispatcher = adr.Dispatcher(self.hw, lambda a, b: a - b)
        dispatcher.step(adr.DriveRequest(1000, connected=True, crank=1, power_adjust=1), False, 1000, .02)
        dispatcher.step(adr.DriveRequest(1020, connected=True, arm=True, crank=1, power_adjust=1), False, 1020, .02)
        self.assertTrue(dispatcher.armed)
        self.assertFalse(any(self.hw.cranks))



class ServoAtomicTest(unittest.TestCase):
    def setUp(self):
        self.hw = Hardware()  # Deliberately has no feedback API.
        self.records = []
        self.logger = SimpleNamespace(emit=lambda *args: self.records.append(args))
        self.servo = adr.ServoMotion(self.hw, 's2', 90, 0, 180, 0, self.logger)
        self.servo.update(None, 1)

    def test_each_step_is_one_relative_command_and_clamps_at_both_limits(self):
        for step in adr.SERVO_STEPS_DEG:
            self.servo.update(1, step)
            self.servo.update(1, step)
            self.servo.update(None, step)
            self.servo.update(-1, step)
            self.servo.update(None, step)
        self.assertEqual([(d, 's2') for step in adr.SERVO_STEPS_DEG for d in (step, -step)], self.hw.adds)
        self.servo.update(1, 60)
        self.servo.update(None, 60)
        self.servo.update(1, 60)
        self.assertEqual((30, 's2'), self.hw.adds[-1])
        self.assertEqual(180, self.servo.position)
        writes = len(self.hw.adds)
        self.servo.update(None, 60)
        self.servo.update(1, 60)
        self.assertEqual(writes, len(self.hw.adds))
        for _ in range(4):
            self.servo.update(None, 60)
            self.servo.update(-1, 60)
        self.assertEqual(0, self.servo.position)
        self.assertTrue(all(0 <= a <= 180 for a, _ in self.hw.servos))

    def test_add_exception_drops_without_pose_change_or_automatic_retry(self):
        with patch.object(self.hw, 'servo_add', side_effect=OSError('write unavailable')) as write:
            for _ in range(200):
                self.servo.update(1, 30)
            self.assertEqual(1, write.call_count)
        self.assertEqual(90, self.servo.position)
        self.assertEqual(1, len(self.records))
        self.assertIn('add failed: write unavailable', self.records[0][2])
        self.servo.update(1, 30)
        self.assertEqual([], self.hw.adds)
        self.servo.update(None, 30)
        self.servo.update(1, 30)
        self.assertEqual([(30, 's2')], self.hw.adds)
        self.assertEqual(120, self.servo.position)

    def test_initial_set_error_waits_for_explicit_retry(self):
        servo = adr.ServoMotion(self.hw, 's3', 90, 0, 180, 0, self.logger)
        with patch.object(self.hw, 'servo', side_effect=OSError('initial write unavailable')) as write:
            for _ in range(20):
                servo.update(0, 30)
            self.assertEqual(1, write.call_count)
        self.assertFalse(servo.initialized)
        servo.update(1, 30)  # Explicit new gesture retries home, without a queued increment.
        self.assertTrue(servo.initialized)
        self.assertEqual([], self.hw.adds)
        servo.update(1, 30)
        self.assertEqual([], self.hw.adds)
        servo.update(0, 30)
        servo.update(1, 30)
        self.assertEqual([(30, 's3')], self.hw.adds)

    def test_native_return_is_not_feedback_and_idle_never_recommands(self):
        calls = []
        self.hw.servo_add = lambda *args: calls.append(args) or False
        self.servo.update(1, 15)
        self.assertEqual(105, self.servo.position)
        for direction in (1,) * 200 + (0,) * 200:
            self.servo.update(direction, 15)
        self.assertEqual([(15, 's2')], calls)
        self.assertEqual([(90, 's2')], self.hw.servos)
        self.assertEqual([], self.records)

    def test_vendor_namespace_uses_cyberpi_mbot2_add_without_getter(self):
        calls = []
        native_servo = SimpleNamespace(servo_set=lambda *args: calls.append(('set', args)),
                                       servo_add=lambda *args: calls.append(('add', args)))
        wheel = SimpleNamespace(drive_speed=lambda *args: None, motor_set=lambda *args: None)
        hw = adr.CyberPiHardware(SimpleNamespace(mbot2=native_servo), None, wheel)
        hw.servo(90, 's2')
        hw.servo_add(-30, 's2')
        hw.servo_add(0, 's2')
        self.assertEqual([('set', (90, 's2')), ('add', (-30, 's2'))], calls)
        native_servo.servo_add = None
        with self.assertRaisesRegex(RuntimeError, 'servo_add'):
            adr.CyberPiHardware(SimpleNamespace(mbot2=native_servo), None, wheel)


class BeamGuardTest(unittest.TestCase):
    def test_geometry_calibration_and_parallel_neutral_clearance(self):
        self.assertEqual((170, 10, 173.5, 0, 10), (adr.GRIP_BEAM_LENGTH_MM,
            adr.GRIP_BEAM_THICKNESS_MM, adr.GRIP_PIVOT_SPACING_MM,
            adr.GRIP_PIVOT_FORWARD_OFFSET_MM, adr.GRIP_MIN_CLEARANCE_MM))
        guard = adr.BeamGuard()
        self.assertEqual(((0, 0), (0, 170)), guard._segment(True, adr.GRIP1_NEUTRAL_DEG))
        self.assertEqual(((173.5, 0), (173.5, 170)), guard._segment(False, adr.GRIP2_NEUTRAL_DEG))
        self.assertTrue(guard.allows(True, 90, 150, 90))
        self.assertFalse(guard.allows(True, 150, 180, 90))
        self.assertFalse(guard.allows(False, 90, 30, 150))

    def test_minimum_surface_gap_includes_both_beam_thicknesses(self):
        guard = adr.BeamGuard()
        with patch.object(adr, 'GRIP_PIVOT_SPACING_MM', 20):
            self.assertTrue(guard.allows(True, 90, 90, 90))
        with patch.object(adr, 'GRIP_PIVOT_SPACING_MM', 19.99):
            self.assertFalse(guard.allows(True, 90, 90, 90))

    def test_sweep_rejects_crossing_even_when_both_endpoints_are_safe(self):
        guard = adr.BeamGuard()
        # Wider fixture makes the arm pass across the other arm and clear it again.
        with patch.object(adr, 'GRIP_PIVOT_SPACING_MM', 230):
            self.assertTrue(guard.allows(True, 120, 120, 30))
            self.assertTrue(guard.allows(True, 180, 180, 30))
            self.assertFalse(guard.allows(True, 120, 180, 30))

    def test_segment_distance_handles_crossings_parallel_and_endpoint_contacts(self):
        self.assertEqual(0, adr.segment_distance_sq((0, 0), (10, 10), (0, 10), (10, 0)))
        self.assertEqual(9, adr.segment_distance_sq((0, 0), (10, 0), (0, 3), (10, 3)))
        self.assertEqual(4, adr.segment_distance_sq((0, 0), (10, 0), (12, 0), (20, 0)))
        self.assertEqual(0, adr.segment_distance_sq((0, 0), (10, 0), (5, 0), (20, 0)))
        self.assertEqual(25, adr.segment_distance_sq((0, 0), (0, 0), (3, 4), (3, 4)))


class LauncherTest(unittest.TestCase):
    def setUp(self):
        self.now = 0
        self.hw = Hardware()
        self.manip = adr.Manipulators(self.hw, lambda a, b: a - b)
        self.manip.update(0, 0, 0, 0, .02)
        self.manip.grip_left.command(105)
        self.manip.grip_right.command(75)
        self.hw.adds.clear()
        # Synthetic deterministic fixtures, never robot calibration defaults.
        self.mechanics = dict(adr.LAUNCH_MECHANICS, overtravel_deg=35, travel_max_deg=120)
        self.calibration = dict(adr.LAUNCH_CALIBRATION, home_duration_us=100000,
            response_delay_us=10000, loaded_speed_rad_s=2, loaded_accel_rad_s2=8,
            unloaded_speed_rad_s=3, unloaded_accel_rad_s2=8, grip_latency_us=20000,
            ball_detach_delay_us=5000, braking_distance_deg=10, braking_duration_us=200000,
            release_tolerance_deg=1, friction_nm=.01)
        self.model = adr.LaunchModel(self.mechanics, self.calibration)
        self.records = []
        self.launch = adr.Launcher(self.manip, SimpleNamespace(emit=lambda *args: self.records.append(args)),
            clock=lambda: self.now, diff=lambda a, b: a - b, model=self.model)

    def advance(self, now):
        self.now = int(math.ceil(now))
        self.launch.service()

    def stroke(self):
        self.assertTrue(self.launch.start())
        self.advance(self.launch.home_due_us)
        self.advance(self.launch.stroke_us)
        self.assertEqual('RELEASE_PENDING', self.launch.state)

    def finish(self):
        self.advance(self.launch.release_due_us)
        self.advance(self.launch.ball_detach_est_us)
        self.advance(self.launch.brake_us)
        self.advance(self.launch.complete_us)

    def test_hardware_profile_si_conversion_and_computed_inertia(self):
        self.assertEqual(('MECDS-150', 6., 16.5, 1.61809725, .19, 5.5116), self.model.hardware_profile)
        self.assertAlmostEqual(16.5 * 9.80665 / 100, adr.PITCH_STALL_TORQUE_NM)
        self.assertAlmostEqual(math.pi / 3 / .19, adr.PITCH_NOLOAD_SPEED_RAD_S, places=4)
        with self.assertRaises(TypeError):
            self.model.hardware_profile[1] = 12
        self.assertAlmostEqual(.0006743333333333334, self.model.inertia())
        self.assertAlmostEqual(.0002408333333333334, self.model.inertia(False))
        self.model.mechanics['arm_length_m'] = .215
        self.assertAlmostEqual(.0010785833333333334, self.model.inertia())
        expected = (adr.PITCH_STALL_TORQUE_NM - .01) / self.model.inertia()
        self.assertAlmostEqual(expected, self.model.peak_zero_speed_reference(0))

    def test_ratings_do_not_determine_operating_acceleration_or_speed(self):
        expected = self.model.plan(0)
        with patch.object(adr, 'PITCH_STALL_TORQUE_NM', 999), patch.object(adr, 'PITCH_NOLOAD_SPEED_RAD_S', 999):
            self.assertEqual(expected, self.model.plan(0))
        self.model.calibration['loaded_accel_rad_s2'] = None
        with self.assertRaisesRegex(ValueError, 'loaded_accel'):
            self.model.plan(0)

    def test_zero_acceleration_coasts_and_gravity_is_signed_from_vertical(self):
        self.assertEqual(2, adr.motion_distance(1, 0, 3, 2))
        self.assertEqual(1, adr.motion_time(2, 0, 3, 2))
        self.model.calibration['friction_nm'] = 0
        self.assertLess(self.model.net_acceleration(0, -math.pi / 4), 0)
        self.assertGreater(self.model.net_acceleration(0, math.pi / 4), 0)
        self.model.calibration['unloaded_accel_rad_s2'] = 0
        self.stroke()
        self.finish()
        self.assertEqual('COMPLETE', self.launch.state)

    def test_full_sequence_crosses_release_without_pitch_stop_and_finishes_quietly(self):
        self.stroke()
        self.assertEqual([(-35, 's1'), (70, 's1')], self.hw.adds)
        self.advance(self.launch.release_due_us - 1)
        self.assertEqual(2, len(self.hw.adds))
        self.advance(self.launch.release_due_us)
        self.assertEqual([(-35, 's1'), (70, 's1'), (-15, 's2'), (15, 's3')], self.hw.adds)
        self.assertEqual('RELEASE_PENDING', self.launch.state)
        self.assertGreater(self.launch.ball_detach_est_us, max(self.launch.latch_open_est_us))
        self.assertEqual(self.model.inertia(), self.launch.current_inertia)
        self.advance(self.launch.ball_detach_est_us)
        self.assertEqual(self.model.inertia(False), self.launch.current_inertia)
        self.advance(self.launch.brake_us)
        self.assertEqual('DECELERATING', self.launch.state)
        self.advance(self.launch.complete_us)
        expected = ['HOMING', 'LOAD', 'ARMED', 'ACCELERATING', 'RELEASE_PENDING', 'RELEASED',
                    'OVERRUN', 'DECELERATING', 'COMPLETE']
        self.assertEqual(expected, [event[0] for event in self.launch.events])
        self.assertEqual(sorted(t for _, t in self.launch.events), [t for _, t in self.launch.events])
        writes = list(self.hw.servos)
        for _ in range(100):
            self.advance(self.now + 10000)
        self.assertEqual(writes, self.hw.servos)
        self.assertEqual(0, self.manip.last_manual_pitch)
        self.assertTrue(any('ball detach EST=' in record[2] for record in self.records))

    def test_latency_prediction_matches_constant_acceleration_and_speed_cap(self):
        plan = self.model.plan(0)
        self.assertAlmostEqual(-2 * .025 * 180 / math.pi, plan['trigger_deg'], places=3)
        c = self.model.calibration
        c['loaded_speed_rad_s'] = c['unloaded_speed_rad_s'] = 10
        plan = self.model.plan(0)
        trigger_s, latency_s = plan['trigger_us'] / 1e6, plan['latency_us'] / 1e6
        omega = c['loaded_accel_rad_s2'] * trigger_s
        predicted = plan['trigger_deg'] * math.pi / 180 + omega * latency_s + .5 * c['loaded_accel_rad_s2'] * latency_s ** 2
        self.assertAlmostEqual(0, predicted, places=5)

    def test_mode_edges_capture_last_manual_pose_before_homing_and_ignore_hold(self):
        self.manip.last_manual_pitch = 20
        self.launch.inputs(False, False)
        self.launch.inputs(False, True)
        self.launch.inputs(False, True)
        self.assertFalse(self.launch.vertical)
        self.assertEqual([], self.hw.adds)
        self.assertTrue(self.launch.inputs(True, False))
        self.assertFalse(self.launch.inputs(True, False))
        self.stroke()
        self.assertEqual(20, self.launch.plan['release_deg'])
        self.assertEqual(55, self.manip.pitch.position)
        self.finish()
        self.assertEqual(20, self.manip.last_manual_pitch)
        self.assertTrue(self.launch.start())
        self.assertEqual(20, self.launch.plan['release_deg'])

    def test_missing_calibration_or_geometry_inhibits_before_any_servo_write(self):
        self.launch.model = adr.LaunchModel()
        self.assertFalse(self.launch.start())
        self.assertEqual('INHIBITED', self.launch.state)
        self.assertEqual([], self.hw.adds)
        self.assertFalse(self.launch.start())
        self.launch.model = self.model
        self.assertFalse(self.launch.start())
        self.launch.reset_fault()
        self.assertTrue(self.launch.start())

    def test_invalid_mechanics_inhibit_shooting_without_breaking_manual_control(self):
        model = adr.LaunchModel(dict(self.mechanics, arm_length_m=None), self.calibration)
        launch = adr.Launcher(self.manip, clock=lambda: self.now,
                              diff=lambda a, b: a - b, model=model)
        self.assertFalse(launch.start())
        self.assertEqual('INHIBITED', launch.state)
        self.assertEqual([], self.hw.adds)
        self.manip.update(0, 0, 1, 0, .02)
        self.assertEqual([(1, 's1')], self.hw.adds)

    def test_invalid_timing_and_stopping_profiles_are_inhibited(self):
        for key, value in (('grip_latency_us', 1000000), ('loaded_speed_rad_s', 0),
                           ('loaded_accel_rad_s2', float('nan')), ('home_duration_us', False),
                           ('unloaded_speed_rad_s', 1), ('braking_distance_deg', 35),
                           ('braking_duration_us', -1), ('release_tolerance_deg', None)):
            with self.subTest(key=key):
                model = adr.LaunchModel(self.mechanics, dict(self.calibration, **{key: value}))
                with self.assertRaises(ValueError):
                    model.plan(0)
        for changes in ({'overtravel_deg': 0}, {'travel_max_deg': 35}, {'start_deg': -60}, {'arm_length_m': -1}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                adr.LaunchModel(dict(self.mechanics, **changes), self.calibration).plan(0)
        with self.assertRaises(ValueError):
            self.model.plan(-40)

    def test_late_release_faults_without_opening_and_inhibits_held_or_new_shots(self):
        self.stroke()
        self.advance(self.launch.release_due_us + 20000)
        self.assertEqual('FAULT', self.launch.state)
        self.assertIn('missed release timing', self.launch.reason)
        self.assertEqual([(-35, 's1'), (70, 's1')], self.hw.adds)
        self.assertFalse(self.launch.start())
        self.launch.service()
        self.assertEqual(2, len(self.hw.adds))

    def test_slow_native_release_is_faulted_without_retry_or_pitch_correction(self):
        self.stroke()
        add = self.hw.servo_add
        def slow_add(delta, port):
            add(delta, port)
            self.now += 20000
        self.hw.servo_add = slow_add
        self.advance(self.launch.release_due_us)
        self.assertEqual('FAULT', self.launch.state)
        self.assertIn('dispatch exceeded', self.launch.reason)
        self.assertEqual([(-35, 's1'), (70, 's1')], [v for v in self.hw.adds if v[1] == 's1'])
        writes = len(self.hw.adds)
        self.launch.service()
        self.assertEqual(writes, len(self.hw.adds))

    def test_release_preflight_and_partial_native_failure_fault_without_retry(self):
        with patch.object(self.manip, 'release_allowed', return_value=False):
            self.assertFalse(self.launch.start())
        self.assertEqual([], self.hw.adds)
        self.launch.reset_fault()
        self.stroke()
        add = self.hw.servo_add
        def broken_right(delta, port):
            if port == 's3':
                raise OSError('right write failed')
            add(delta, port)
        self.hw.servo_add = broken_right
        self.advance(self.launch.release_due_us)
        self.assertEqual('FAULT', self.launch.state)
        self.assertEqual((90, 75), (self.manip.grip_left.position, self.manip.grip_right.position))
        self.assertIn('partial release', self.launch.reason)

    def test_microsecond_clock_wrap_preserves_deadlines(self):
        period = 1 << 30
        self.now = period - 50000
        self.launch.clock = lambda: self.now % period
        self.launch.diff = lambda a, b: (a - b + period // 2) % period - period // 2
        epoch = self.now
        self.assertTrue(self.launch.start())
        self.advance(epoch + self.launch.home_due_us)
        self.advance(epoch + self.launch.stroke_us)
        self.advance(epoch + self.launch.release_due_us)
        self.advance(epoch + self.launch.ball_detach_est_us)
        self.advance(epoch + self.launch.complete_us)
        self.assertEqual('COMPLETE', self.launch.state)
        self.assertTrue(all(t >= 0 for _, t in self.launch.events))

    def test_dispatcher_abort_on_stop_staleness_manual_override_and_shutdown(self):
        for cause in ('stop', 'stale', 'manual', 'flap'):
            with self.subTest(cause=cause):
                self.setUp()
                d = adr.Dispatcher(self.hw, lambda a, b: a - b)
                d.manipulators, d.launcher, d.armed = self.manip, self.launch, True
                d.step(adr.DriveRequest(1000, connected=True), False, 1000, .02)
                d.step(adr.DriveRequest(1020, connected=True, shoot=True), False, 1020, .02)
                self.assertTrue(self.launch.active())
                writes = len(self.hw.adds)
                r = adr.DriveRequest(1040, connected=True, estop=cause == 'stop', pitch=1 if cause == 'manual' else 0, flap=cause == 'flap')
                d.step(r, cause == 'stop', 1241 if cause == 'stale' else 1040, .02)
                self.assertEqual('FAULT', self.launch.state)
                self.assertEqual(writes, len(self.hw.adds))
        self.launch.reset_fault()
        self.assertTrue(self.launch.start())
        self.launch.cancel('control worker shutdown')
        self.assertEqual('FAULT', self.launch.state)

    def test_disarmed_shoot_cannot_queue_and_fault_requires_explicit_start_reset(self):
        d = adr.Dispatcher(self.hw, lambda a, b: a - b)
        d.manipulators, d.launcher = self.manip, self.launch
        for stamp, values in ((1000, {}), (1020, {'shoot': True}),
                              (1040, {'shoot': True, 'arm': True})):
            d.step(adr.DriveRequest(stamp, connected=True, **values), False, stamp, .02)
        self.assertFalse(d.armed)
        d.step(adr.DriveRequest(1060, connected=True), False, 1060, .02)
        d.step(adr.DriveRequest(1080, connected=True, arm=True), False, 1080, .02)
        self.assertEqual('IDLE', self.launch.state)
        self.launch.fault('timing failure')
        for stamp, shoot in ((1100, False), (1120, True)):
            d.step(adr.DriveRequest(stamp, connected=True, shoot=shoot), False, stamp, .02)
        self.assertEqual('FAULT', self.launch.state)
        d.step(adr.DriveRequest(1140, connected=True, arm=True), False, 1140, .02)
        self.assertEqual('IDLE', self.launch.state)
        self.assertEqual([], self.hw.adds)

    def test_release_mode_can_be_selected_before_arming_without_actuation(self):
        d = adr.Dispatcher(self.hw, lambda a, b: a - b)
        for stamp, mode in ((1000, False), (1020, True), (1040, True)):
            d.step(adr.DriveRequest(stamp, connected=True, release_mode=mode), False, stamp, .02)
        self.assertFalse(d.armed)
        self.assertFalse(d.launcher.vertical)
        self.assertEqual([], self.hw.adds)

    def test_worker_services_launch_between_gamepad_reads_without_extra_wheel_writes(self):
        now, samples, wheel, sleeps, launchers = [0], [], [], [], []
        state, hw = adr.WorkerState(), Hardware()
        def sample():
            phase = len(samples)
            samples.append(now[0])
            return adr.DriveRequest(now[0] // 1000, connected=True, arm=phase == 1,
                grip_left=1 if phase == 2 else 0, grip_right=-1 if phase == 2 else 0, shoot=phase == 4)
        hw.sample = sample
        hw.gamepad_wait_reason = 'waiting'
        hw.drive = lambda *args: wheel.append(args)
        def sleep(ms):
            now[0] += ms * 1000
            sleeps.append(ms)
            if now[0] > 2000000 or (launchers and launchers[0].state == 'COMPLETE'):
                state.close()
        clock = SimpleNamespace(ticks_ms=lambda: now[0] // 1000, ticks_us=lambda: now[0],
                                ticks_diff=lambda a, b: a - b, sleep_ms=sleep)
        factory = adr.Launcher
        def launcher(*args, **kwargs):
            obj = factory(*args, **kwargs, model=self.model)
            launchers.append(obj)
            return obj
        with patch.object(adr, 'time', clock), patch.object(adr, 'Launcher', launcher):
            adr.dispatch_loop(hw, state)
        self.assertIsNone(state.failure)
        self.assertEqual('COMPLETE', launchers[0].state)
        self.assertLess(len(wheel), len(samples) + 1)
        self.assertGreater(sleeps.count(1), 100)
        self.assertTrue(all(b - a >= 20000 for a, b in zip(samples, samples[1:])))
        self.assertEqual([(-35, 's1'), (70, 's1')], [v for v in hw.adds if v[1] == 's1'])


class GamepadLinkTest(unittest.TestCase):
    def setUp(self):
        self.online = False
        self.pad = SimpleNamespace(is_online=lambda: self.online,
            is_connected=lambda: True, get_joystick=lambda name: 0,
            is_key_pressed=lambda name: False)
        motors = SimpleNamespace(drive_speed=lambda *args: None, motor_set=lambda *args: None,
            servo_set=lambda *args: None, servo_add=lambda *args: None)
        cpi = SimpleNamespace(mbot2=motors, controller=SimpleNamespace(is_press=lambda key: False))
        self.hw = adr.CyberPiHardware(cpi, self.pad, motors)
        clock_patch = patch.object(adr.time, 'ticks_ms', lambda: 1000, create=True)
        clock_patch.start()
        self.addCleanup(clock_patch.stop)

    def test_cyberos_online_status_controls_local_input(self):
        # A nonexistent/foreign is_connected API must not override CyberOS status.
        self.assertEqual('is_online', adr.GAMEPAD_LINK_METHOD)
        self.assertFalse(self.hw.sample().connected)
        self.online = True
        self.assertTrue(self.hw.sample().connected)
        self.online = False
        self.assertFalse(self.hw.sample().connected)

    def test_missing_or_failing_online_status_keeps_waiting(self):
        self.pad.is_online = None
        self.assertFalse(self.hw.sample().connected)
        self.assertEqual('gamepad link API unavailable', self.hw.gamepad_wait_reason)
        def unavailable():
            raise OSError('link unavailable')
        self.pad.is_online = unavailable
        self.assertFalse(self.hw.sample().connected)
        self.assertIn('link unavailable', self.hw.gamepad_wait_reason)


class DispatchSchedulingTest(unittest.TestCase):
    def setUp(self):
        self.hw = Hardware()
        self.records = []
        self.logger = SimpleNamespace(emit=lambda *record: self.records.append(record))
        self.controller = adr.Dispatcher(self.hw, lambda a, b: a - b, logger=self.logger)

    def step(self, request=None, now=1000, dt=.02):
        self.controller.step(request, bool(request and (request.estop or not request.connected)), now, dt)

    def test_sticks_drive_after_neutral_start_without_deadman(self):
        self.step(adr.DriveRequest(1000, 50, -50, connected=True))
        self.assertFalse(self.controller.armed)
        self.assertEqual((0, 0), self.hw.power)
        self.step(adr.DriveRequest(1020, connected=True), now=1020)
        self.step(adr.DriveRequest(1040, connected=True, arm=True), now=1040)
        self.assertTrue(self.controller.armed)
        self.step(adr.DriveRequest(1060, 50, -50, connected=True), now=1060)
        self.assertEqual((50, -50), self.hw.power)
        # A legacy deadman value cannot enable or stop the local drive path.
        self.step(adr.DriveRequest(1080, 50, -50, connected=True, deadman=True), now=1080)
        self.step(adr.DriveRequest(1100, 50, -50, connected=True, deadman=False), now=1100)
        self.assertEqual((50, -50), self.hw.power)
        self.step(adr.DriveRequest(1120, connected=True, estop=True), now=1120)
        self.assertFalse(self.controller.armed)
        self.assertEqual((0, 0), self.hw.power)

    def test_disconnected_stop_remains_stopped_between_input_samples(self):
        self.step(adr.DriveRequest(1000))
        self.step(now=1020)
        self.step(now=1300)
        self.step(adr.DriveRequest(1320), now=1320)
        self.assertEqual([('WARN', 'drive', 'disconnect / emergency stop')], self.records)
        self.assertFalse(self.controller.armed)
        self.assertEqual((0, 0), self.hw.power)

    def test_stop_requires_new_neutral_release_before_arm(self):
        self.step(adr.DriveRequest(1000, connected=True))
        self.step(adr.DriveRequest(1020, connected=True, arm=True, deadman=True), now=1020)
        self.assertTrue(self.controller.armed)
        self.step(adr.DriveRequest(1040, estop=True), now=1040)
        self.step(adr.DriveRequest(1060, connected=True, arm=True, deadman=True), now=1060)
        self.assertFalse(self.controller.armed)
        self.step(adr.DriveRequest(1080, connected=True), now=1080)
        self.step(adr.DriveRequest(1100, connected=True, arm=True), now=1100)
        self.assertTrue(self.controller.armed)

    def test_latest_motion_expires_without_timestamp_refresh(self):
        self.step(adr.DriveRequest(1000, connected=True))
        self.step(adr.DriveRequest(1020, connected=True, arm=True), now=1020)
        request = adr.DriveRequest(1040, 30, 30, True, True)
        self.step(request, now=1040)
        self.step(now=1060)
        self.assertGreater(self.hw.power[0], 0)
        self.step(now=1241)
        self.assertEqual(1040, request.stamp)
        self.assertEqual('input timeout', self.controller.reason)
        self.assertFalse(self.controller.armed)
        self.assertEqual((0, 0), self.hw.power)

    def test_parked_control_gap_guard_keeps_current_fresh_input_profile(self):
        self.controller.armed = True
        self.step(adr.DriveRequest(1000, 30, 30, True, True), dt=(adr.CONTROL_GAP_MS + 1) / 1000)
        self.assertEqual('running', self.controller.reason)
        self.assertTrue(self.controller.armed)
        self.assertEqual((30, 30), self.hw.power)

    def test_worker_keeps_sampling_and_stopping_while_screen_is_blocked(self):
        entered, release = threading.Event(), threading.Event()
        now, iteration, writes = [1000], [0], []
        state = adr.WorkerState()
        def sleep(ms):
            now[0] += ms
            iteration[0] += 1
            if iteration[0] >= 5:
                state.close()
        def draw(*args, **kwargs):
            entered.set()
            if not release.wait(2):
                raise RuntimeError('test display release timed out')
        def sample():
            phase = iteration[0]
            return adr.DriveRequest(now[0], 20 if phase == 2 else 0, 0,
                                    connected=phase < 3, arm=phase == 1, deadman=True)
        hw = Hardware()
        hw.sample = sample
        hw.gamepad_wait_reason = 'gamepad not connected'
        hw.drive = lambda left, right: writes.append((left, right))
        stop = hw.stop
        hw.stop = lambda: (writes.append((0, 0)), stop())
        clock = SimpleNamespace(ticks_ms=lambda: now[0], ticks_diff=lambda a, b: a - b, sleep_ms=sleep)
        with patch.object(adr, 'time', clock):
            log = adr.KernelLog(clock.ticks_ms, clock.ticks_diff)
            log.screen = SimpleNamespace(show_label=draw)
            display = threading.Thread(target=log.pump, daemon=True)
            display.start()
            try:
                self.assertTrue(entered.wait(1))
                adr.dispatch_loop(hw, state, log)
                self.assertTrue(state.is_done())
                self.assertEqual(5, iteration[0])
                self.assertTrue(any(left > 0 for left, right in writes))
                self.assertEqual((0, 0), writes[-1])
                self.assertTrue(display.is_alive())
            finally:
                release.set()
                display.join(1)

    def test_slow_input_cannot_issue_motion_and_errors_keep_worker_alive(self):
        now, iteration, writes = [1000], [0], []
        state = adr.WorkerState()
        hw = Hardware()
        hw.gamepad_wait_reason = 'waiting'
        hw.drive = lambda *args: writes.append(args)
        def sample():
            phase = iteration[0]
            if phase == 0:
                raise OSError('temporary input error')
            if phase == 1:
                return adr.DriveRequest(now[0], connected=True)
            if phase == 2:
                return adr.DriveRequest(now[0], connected=True, arm=True)
            stamp = now[0]
            now[0] += adr.REQUEST_TIMEOUT_MS + 1
            return adr.DriveRequest(stamp, 20, 20, True, True)
        hw.sample = sample
        def sleep(ms):
            now[0] += ms
            iteration[0] += 1
            if iteration[0] == 4:
                state.close()
        clock = SimpleNamespace(ticks_ms=lambda: now[0], ticks_diff=lambda a, b: a - b, sleep_ms=sleep)
        with patch.object(adr, 'time', clock):
            adr.dispatch_loop(hw, state)
        self.assertEqual(4, iteration[0])
        self.assertTrue(state.is_done())
        self.assertFalse(any(left or right for left, right in writes))


class FirmwareBootTest(unittest.TestCase):
    def setUp(self):
        self.clock = SimpleNamespace(ticks_ms=lambda: 1000,
                                     ticks_diff=lambda a, b: a - b,
                                     sleep_ms=lambda ms: time.sleep(ms / 1000))
        self.time_patch = patch.object(adr, 'time', self.clock)
        self.time_patch.start()
        self.addCleanup(self.time_patch.stop)
        self.logger = adr.KernelLog(self.clock.ticks_ms, self.clock.ticks_diff)

    def test_cyberos_program_slot_is_an_application_entry(self):
        self.assertTrue(adr.is_application_entry('main1', 'micropython'))
        self.assertTrue(adr.is_application_entry('main8', 'micropython'))
        self.assertTrue(adr.is_application_entry('__main__', 'micropython'))
        self.assertFalse(adr.is_application_entry('main1', 'cpython'))
        self.assertFalse(adr.is_application_entry('adr', 'micropython'))
        self.assertFalse(adr.is_application_entry('main_tools', 'micropython'))

    def test_runtime_serial_trace_survives_missing_lcd_and_print_errors(self):
        self.logger.emit('INFO', 'custom', 'serial trace')
        output = io.StringIO()
        with patch.object(adr, 'LOG_SERIAL_ENABLED', True), contextlib.redirect_stdout(output):
            self.logger.pump(force=True)
        self.assertIn('[ADL  0.000] INFO custom: serial trace', output.getvalue())
        self.logger.emit('WARN', 'custom', 'unavailable stdout')
        with patch('builtins.print', side_effect=OSError('serial unavailable')):
            self.logger.pump(force=True)

    def test_runtime_log_burst_is_bounded_and_uses_one_serial_write(self):
        for i in range(adr.LOG_BATCH_SIZE + 2):
            self.logger.emit('INFO', 'burst', str(i))
        with patch('builtins.print') as output:
            self.logger.pump()
        self.assertEqual(1, output.call_count)
        self.assertTrue(output.call_args.args[0].startswith('[ADL '))
        self.assertEqual(2, len(self.logger.pending))
        with patch('builtins.print') as output:
            self.logger.pump(force=True)
        self.assertEqual(2, output.call_count)
        self.assertEqual([], self.logger.pending)

    def test_runtime_logs_do_not_repaint_ready_or_waiting_screen(self):
        frames = []
        self.logger.screen = SimpleNamespace(show_label=lambda *args, **kwargs: frames.append(args))
        with patch('builtins.print'):
            self.logger.pump()
            self.assertIn('GAMEPAD READY', frames[-1][0])
            self.assertIn('Ly/Ry: drive', frames[-1][0])
            self.assertIn('Lx/Rx: beams', frames[-1][0])
            self.assertIn('L1/R1: pitch', frames[-1][0])
            self.logger.emit('WARN', 'drive', 'a timing diagnostic')
            self.logger.last_flush = None
            self.logger.pump()
            self.assertEqual(1, len(frames))
            self.logger.set_gamepad_waiting(True)
            self.logger.last_flush = None
            self.logger.pump()
            self.assertEqual('WAITING\nFOR\nGAMEPAD', frames[-1][0])
            self.assertEqual(16, frames[-1][1])
            self.logger.emit('WARN', 'gamepad', 'still waiting')
            self.logger.last_flush = None
            self.logger.pump()
            self.assertEqual(2, len(frames))

    def test_selected_step_redraws_ready_screen_once_on_main(self):
        frames = []
        self.logger.screen = SimpleNamespace(show_label=lambda *args, **kwargs: frames.append(args))
        with patch('builtins.print'):
            self.logger.pump()
            self.assertIn('Step: 1 deg L2-/R2+', frames[-1][0])
            self.logger.set_servo_step(60)
            self.assertEqual(1, len(frames))  # Publishing never calls native display.
            self.logger.last_flush = None
            self.logger.pump()
            self.assertIn('Step: 60 deg L2-/R2+', frames[-1][0])
            self.logger.last_flush = None
            self.logger.pump()
            self.assertEqual(2, len(frames))
            self.logger.set_gamepad_waiting(True)
            self.logger.last_flush = None
            self.logger.pump()
            self.logger.set_servo_step(2)
            self.logger.last_flush = None
            self.logger.pump()
            self.assertEqual(3, len(frames))
            self.assertEqual('WAITING\nFOR\nGAMEPAD', frames[-1][0])
            self.logger.set_gamepad_waiting(False)
            self.logger.last_flush = None
            self.logger.pump()
            self.assertIn('Step: 2 deg L2-/R2+', frames[-1][0])

    def test_launch_display_publishes_mode_and_run_once_without_servo_feedback(self):
        frames = []
        self.logger.screen = SimpleNamespace(show_label=lambda *args, **kwargs: frames.append(args))
        with patch('builtins.print'):
            self.logger.pump()
            self.assertIn('N1:shoot N2:0deg', frames[-1][0])
            self.assertIn('N4:flap', frames[-1][0])
            self.logger.set_launcher('RUN', False)
            self.assertEqual(1, len(frames))
            self.logger.last_flush = None
            self.logger.pump()
            self.assertIn('N1:shoot N2:manual', frames[-1][0])
            self.assertIn('Shot: RUN', frames[-1][0])
            self.logger.set_launcher('RUN', False)
            self.logger.emit('INFO', 'launch', 'RELEASE_PENDING estimated')
            self.logger.last_flush = None
            self.logger.pump()
            self.assertEqual(2, len(frames))
            self.logger.set_launcher('FAULT', False)
            self.logger.last_flush = None
            self.logger.pump()
            self.assertIn('Shot: FAULT', frames[-1][0])

    def test_field_tool_runtime_is_removed(self):
        for name in ('AdlFieldTool', 'CyberPiBluetooth', 'OptionalFieldTool', 'prepare_field_tool'):
            self.assertFalse(hasattr(adr, name), name)

    def test_boot_marker_works_without_lcd_and_tolerates_console_errors(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            adr.boot_mark('03 importing cyberpi')
        self.assertIn('[ADR boot] 03 importing cyberpi', output.getvalue())
        with patch('builtins.print', side_effect=OSError('host console unavailable')):
            adr.boot_mark('diagnostic failure')

    def test_boot_without_communication_reaches_gamepad_loop_after_zeroing(self):
        self.assert_boot_without_communication()

    def test_bypass_boot_uses_power_api_for_all_zero_writes(self):
        with patch.object(adr, 'PID_BYPASS', True):
            self.assert_boot_without_communication()

    def assert_boot_without_communication(self):
        events, queues = [], []
        def zero(*args): events.append(('zero', args))
        display = SimpleNamespace(show_label=lambda *args, **kwargs: events.append(('screen',)))
        cyberpi = SimpleNamespace(display=display,
            controller=SimpleNamespace(is_press=lambda key: False),
            mbot2=SimpleNamespace(**{'drive_power' if adr.PID_BYPASS else 'drive_speed': zero,
                                    'motor_set': zero, 'servo_set': lambda *args: None, 'servo_add': lambda *args: None}))
        pad = SimpleNamespace(is_online=lambda: events.append(('sample',)) or False)
        def start_worker(fn, args):
            self.assertIs(fn, adr.dispatch_loop)
            self.assertEqual(3, len(args))
            self.assertEqual(('stack', adr.CONTROL_STACK_BYTES), events[-1])
            queues.append(args)
        def sleep(ms):
            if ms == adr.LOG_FLUSH_MS:
                adr.dispatch_loop(*queues[0])
            else:
                queues[0][1].close()
        self.clock.sleep_ms = sleep
        output = io.StringIO()
        with patch.dict(sys.modules, cyberpi=cyberpi, mbot2=cyberpi.mbot2, gamepad=pad), patch.object(
                adr._thread, 'start_new_thread', start_worker), patch.object(
                adr._thread, 'stack_size', lambda size: events.append(('stack', size)) or 0), patch.object(
                adr, 'CyberPiBluetooth', side_effect=AssertionError('BLE must be unused'), create=True):
            with contextlib.redirect_stdout(output):
                adr.main()
        self.assertEqual(['zero', 'zero'], [event[0] for event in events[:2]])
        self.assertIn(('sample',), events)
        self.assertIn(('stack', 0), events)
        self.assertIn('12 input loop ready', output.getvalue())

    def test_dispatch_fault_stops_before_main_prints_original_traceback(self):
        now, phase, queued, traces, events = [1000], [0], [], [], []
        failure = RuntimeError('maximum recursion depth exceeded')
        hw = Hardware()
        hw.gamepad_wait_reason = 'waiting'
        hw.sample = lambda: adr.DriveRequest(now[0], 20 if phase[0] == 2 else 0, 0,
                                             connected=True, arm=phase[0] == 1)
        def drive(left, right):
            if left:
                raise failure
            hw.power = (left, right)
        hw.drive = drive
        stop = hw.stop
        hw.stop = lambda: (events.append('stop'), stop())
        def sleep(ms):
            now[0] += ms
            if ms == adr.LOG_FLUSH_MS:
                adr.dispatch_loop(*queued[0])
            else:
                phase[0] += 1
        self.clock.ticks_ms = lambda: now[0]
        self.clock.sleep_ms = sleep
        mbot = SimpleNamespace(drive_speed=lambda *args: None, motor_set=lambda *args: None)
        cpi = SimpleNamespace(mbot2=mbot, display=SimpleNamespace(show_label=lambda *args, **kwargs: None))
        def trace(exc):
            self.assertTrue(adr.app_state.is_done())
            self.assertFalse(adr.app_state.armed)
            self.assertEqual((0, 0), hw.power)
            self.assertEqual('stop', events[-1])
            traces.append(exc)
        output = io.StringIO()
        with patch.dict(sys.modules, cyberpi=cpi, mbot2=mbot, gamepad=SimpleNamespace()), patch.object(
                adr, 'CyberPiHardware', return_value=hw), patch.object(
                adr._thread, 'start_new_thread', lambda fn, args: queued.append(args)), patch.object(
                adr._thread, 'stack_size', return_value=0), patch.object(
                adr.sys, 'print_exception', trace, create=True), contextlib.redirect_stdout(output):
            adr.main()
        self.assertEqual([failure], traces)
        self.assertEqual('dispatch outputs', adr.app_state.failure_stage)
        self.assertIn('dispatch outputs: maximum recursion depth exceeded', output.getvalue())
        self.assertEqual(2, events.count('stop'))

    def test_stack_allocation_failure_aborts_before_worker_start(self):
        zeros = []
        mbot = SimpleNamespace(drive_speed=lambda *args: zeros.append(('wheel', args)),
                               motor_set=lambda *args: zeros.append(('dc', args)),
                               servo_set=lambda *args: None, servo_add=lambda *args: None)
        cpi = SimpleNamespace(mbot2=mbot, display=SimpleNamespace(show_label=lambda *args, **kwargs: None))
        with patch.dict(sys.modules, cyberpi=cpi, mbot2=mbot, gamepad=SimpleNamespace()), patch.object(
                adr._thread, 'stack_size', side_effect=MemoryError('stack unavailable')), patch.object(
                adr._thread, 'start_new_thread') as start, contextlib.redirect_stdout(io.StringIO()):
            with self.assertRaises(MemoryError):
                adr.main()
        start.assert_not_called()
        self.assertEqual([('wheel', (0, 0)), ('dc', (0, 'm1'))] * 2, zeros)

    def test_boot_zero_error_attempts_all_outputs_and_aborts_before_threads(self):
        attempts = []
        def broken_zero(*args):
            attempts.append(args)
            raise OSError('motor bus unavailable')
        cyberpi = SimpleNamespace(mbot2=SimpleNamespace(drive_speed=broken_zero, motor_set=broken_zero))
        with patch.dict(sys.modules, cyberpi=cyberpi, mbot2=cyberpi.mbot2), patch.object(
                adr._thread, 'start_new_thread', side_effect=AssertionError('unsafe startup')):
            with contextlib.redirect_stdout(io.StringIO()), self.assertRaises(OSError):
                adr.main()
        self.assertEqual([(0, 0), (0, adr.CRANKER_PORT)], attempts)


if __name__ == '__main__': unittest.main()

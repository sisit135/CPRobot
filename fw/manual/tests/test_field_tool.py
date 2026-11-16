# SPDX-License-Identifier: GPL-3.0-only
"""ADR drive, ramped manipulator and safety contracts; parked tests stay commented."""
import importlib.util
from pathlib import Path
import unittest
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
        self.cranks = []
    def stop(self):
        self.power = (0, 0)
        self.cranks.append(0)
    def drive(self, left, right): self.power = (left, right)
    def servo(self, angle, port): self.servos.append((angle, port))
    def crank(self, power): self.cranks.append(power)


# class AdlTest(unittest.TestCase):
#     def setUp(self):
#         self.now = 1000
#         self.ft = adr.AdlFieldTool(lambda: self.now, lambda a, b: a - b)
#         self.hw = Hardware()
#         self.dispatch = adr.Dispatcher(self.hw, lambda a, b: a - b)
#         self.ident = 0
#
#     def command(self, op, *values):
#         self.ident += 1
#         return self.ft.exchange(adr.adl_seal('|'.join(str(x) for x in (1, self.ident, op) + values)))
#
#     def step(self):
#         request = self.ft.sample()
#         self.dispatch.step(request, bool(request and request.estop), self.now, .02)
#         self.ft.armed = self.dispatch.armed
#         self.now += 20
#
#     def arm(self):
#         self.command(1)
#         self.step(); self.step(); self.step()
#         self.assertTrue(self.ft.armed)
#
#     def test_shared_crc_golden_and_bounds(self):
#         self.assertEqual(0x29b1, adr.adl_crc('123456789'))
#         self.assertEqual((42, 2, [-60, 60, 1]), adr.adl_decode(adr.adl_seal('1|42|2|-60|60|1')))
#         for body in ['1|42|2|-61|60|1', '1|42|4|0|90', '1|42|5|2', '1|42|99', '2|42|1', '1|-1|1', '1|42|1|0']:
#             with self.assertRaises(ValueError): adr.adl_decode(adr.adl_seal(body))
#         with self.assertRaises(ValueError): adr.adl_decode('1|42|1|0000')
#
#     def test_drive_requires_neutral_arm_then_fresh_deadman_and_times_out(self):
#         self.assertIn('|1|0|', self.command(2, 30, 30, 1))
#         self.arm(); self.command(2, 30, 30, 1); self.step()
#         self.assertGreater(self.hw.power[0], 0)
#         self.now += adr.REQUEST_TIMEOUT_MS + 1
#         self.step()
#         self.assertFalse(self.dispatch.armed)
#         self.assertEqual((0, 0), self.hw.power)
#
#     def test_duplicate_packet_never_refreshes_motion_age(self):
#         self.arm()
#         wire = adr.adl_seal('1|123|2|30|30|1')
#         first = self.ft.exchange(wire); self.step()
#         stamp = self.ft.request.stamp
#         self.now += 210
#         self.assertEqual(first, self.ft.exchange(wire))
#         self.assertEqual(stamp, self.ft.request.stamp)
#         self.step(); self.assertEqual((0, 0), self.hw.power)
#
#     def test_status_polling_never_renews_motion(self):
#         self.arm(); self.command(2, 30, 30, 1); stamp = self.ft.request.stamp
#         for _ in range(12): self.command(6); self.step()
#         self.assertEqual(stamp, self.ft.request.stamp)
#         self.assertFalse(self.dispatch.armed)
#
#     def test_stop_latches_even_if_followed_by_drive_before_main_sample(self):
#         self.arm(); self.command(2, 30, 30, 1); self.step()
#         self.command(3); self.command(2, 30, 30, 1); self.step()
#         self.assertFalse(self.dispatch.armed); self.assertEqual((0, 0), self.hw.power)
#
#     def test_return_to_gamepad_emits_stop_before_releasing_ownership(self):
#         self.arm(); self.command(7)
#         self.assertTrue(self.ft.sample().estop)
#         self.assertIsNone(self.ft.sample())
#
#     def test_native_bluetooth_fragmentation_raw_mode_and_bytearray_writes(self):
#         class Communication:
#             def __init__(self): self.chunks = []; self.sent = []; self.disabled = []; self.enabled = []
#             def disable_channel_default(self, channel): self.disabled.append(channel)
#             def enable_channel_default(self, channel): self.enabled.append(channel)
#             def read(self, channel):
#                 self.assert_channel = channel
#                 return self.chunks.pop(0) if self.chunks else bytearray()
#             def send(self, channel, data):
#                 if not isinstance(data, bytearray): raise AssertionError('Native send requires bytearray')
#                 self.sent.append(bytes(data))
#         api = Communication()
#         bt = adr.CyberPiBluetooth(_thread.allocate_lock(), self.ft, lambda: self.now, lambda a, b: a - b, api)
#         wire = (adr.adl_seal('1|42|6') + '\n').encode()
#         api.chunks = [bytearray(wire[:3]), bytearray(wire[3:])]
#         bt.pump()
#         self.assertEqual(['bluetooth'], api.disabled)
#         self.assertEqual((adr.adl_seal('1|42|6|0|0') + '\n').encode(), b''.join(api.sent))
#         self.assertFalse(self.ft.remote)
#         api.chunks = [bytearray(b'bad|0000\n' + b'x' * 100), bytearray(b'x' * 65 + b'\n' + wire)]
#         bt.pump()
#         self.assertEqual(1, bt.rejected)
#         self.assertLessEqual(len(bt.buffer), 160)
#         bt.close(); self.assertEqual(['bluetooth'], api.enabled)
#
#
class CodecTest(unittest.TestCase):
    def test_shared_crc_golden_and_bounds(self):
        self.assertEqual(0x29b1, adr.adl_crc('123456789'))
        self.assertEqual((42, 2, [-60, 60, 1]), adr.adl_decode(adr.adl_seal('1|42|2|-60|60|1')))
        for body in ['1|42|2|-61|60|1', '1|42|4|0|90', '1|42|5|2', '1|42|99', '2|42|1', '1|-1|1', '1|42|1|0']:
            with self.assertRaises(ValueError): adr.adl_decode(adr.adl_seal(body))
        with self.assertRaises(ValueError): adr.adl_decode('1|42|1|0000')


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

    def test_first_arm_establishes_open_up_reference_once(self):
        self.assertEqual([(30, 's1'), (150, 's2'), (45, 's3')], self.hw.servos)
        self.step()
        self.assertEqual(3, len(self.hw.servos))

    def test_grip_pair_ramps_together_reverses_and_holds_on_release(self):
        for _ in range(10):
            self.step(grip=adr.GRIP1_CLOSED)
        manip = self.dispatch.manipulators
        self.assertAlmostEqual(48, manip.grip)
        pose, writes = manip.grip, len(self.hw.servos)
        self.step()
        self.assertEqual(pose, manip.grip)
        self.assertEqual(writes, len(self.hw.servos))
        self.step(grip=adr.GRIP1_OPEN)
        self.assertAlmostEqual(pose - 1.8, manip.grip)
        for _ in range(40):
            self.step(grip=adr.GRIP1_CLOSED)
        self.assertEqual(90, manip.grip)
        for _ in range(40):
            self.step(grip=adr.GRIP1_OPEN)
        self.assertEqual(30, manip.grip)
        writes = [call for call in self.hw.servos if call[1] in ('s1', 's2')]
        previous = 30
        for a, b in zip(writes[::2], writes[1::2]):
            self.assertEqual(('s1', 's2'), (a[1], b[1]))
            self.assertEqual(180, a[0] + b[0])
            self.assertLessEqual(abs(a[0] - previous), 2)
            self.assertTrue(30 <= a[0] <= 90 and 90 <= b[0] <= 150)
            previous = a[0]

    def test_pitch_holds_reverses_and_clamps_robot_envelope(self):
        manip = self.dispatch.manipulators
        self.step(pitch=1, grip=adr.GRIP1_CLOSED)
        self.assertAlmostEqual(1.2, manip.pitch)
        self.assertAlmostEqual(31.8, manip.grip)
        self.step()
        self.assertAlmostEqual(1.2, manip.pitch)
        for _ in range(200):
            self.step(pitch=-1)
        self.assertEqual(-45, manip.pitch)
        self.assertEqual((0, 's3'), self.hw.servos[-1])
        for _ in range(200):
            self.step(pitch=1)
        self.assertEqual(135, manip.pitch)
        self.assertEqual((180, 's3'), self.hw.servos[-1])
        angles = [a for a, port in self.hw.servos if port == 's3']
        self.assertTrue(all(0 <= a <= 180 for a in angles))
        self.assertTrue(all(abs(a - b) <= 2 for a, b in zip(angles, angles[1:])))

    def test_manipulator_rates_follow_elapsed_time_and_grip_limits(self):
        manip = self.dispatch.manipulators
        self.step(dt=.04, pitch=1, grip=1000)
        self.assertAlmostEqual(2.4, manip.pitch)
        self.assertAlmostEqual(33.6, manip.grip)
        for _ in range(40):
            self.step(grip=1000)
        self.assertEqual(90, manip.grip)
        for _ in range(40):
            self.step(grip=-1000)
        self.assertEqual(30, manip.grip)
        self.assertEqual([(30, 's1'), (150, 's2')], self.hw.servos[-2:])

    def test_shoot_tap_aims_from_either_side_then_releases_both_arms(self):
        manip = self.dispatch.manipulators
        for start_direction, expected_pitch in ((-1, -45), (1, 135)):
            with self.subTest(start=expected_pitch):
                for _ in range(200):
                    self.step(pitch=start_direction, grip=adr.GRIP1_CLOSED)
                self.assertEqual(expected_pitch, manip.pitch)
                self.assertEqual(90, manip.grip)
                self.step(shoot=True)
                self.assertEqual('aim', manip.shoot_phase)
                for _ in range(100):
                    if manip.shoot_phase != 'aim':
                        break
                    self.step()
                    self.assertEqual(90, manip.grip)
                self.assertEqual('release', manip.shoot_phase)
                self.assertEqual(45, manip.pitch)
                self.assertEqual((90, 's3'), self.hw.servos[-1])
                self.step()
                self.assertAlmostEqual(88.2, manip.grip)
                for _ in range(40):
                    self.step()
                self.assertIsNone(manip.shoot_phase)
                self.assertEqual((30, 45), (manip.grip, manip.pitch))
                self.assertEqual([(30, 's1'), (150, 's2')], self.hw.servos[-2:])
                self.assertFalse(any(self.hw.cranks))

    def test_held_shoot_overrides_manual_controls_and_waits_for_release(self):
        for _ in range(40):
            self.step(grip=adr.GRIP1_CLOSED)
        for _ in range(150):
            self.step(shoot=True, grip=adr.GRIP1_CLOSED, pitch=1)
        manip = self.dispatch.manipulators
        self.assertEqual((30, 45, 'release'), (manip.grip, manip.pitch, manip.shoot_phase))
        writes = len(self.hw.servos)
        self.step(shoot=True, grip=adr.GRIP1_CLOSED, pitch=-1)
        self.assertEqual(writes, len(self.hw.servos))
        self.step()
        self.assertIsNone(manip.shoot_phase)
        self.step(grip=adr.GRIP1_CLOSED, pitch=-1)
        self.assertGreater(manip.grip, 30)
        self.assertLess(manip.pitch, 45)

    def test_safety_gates_cancel_shoot_and_rearming_preserves_commanded_pose(self):
        for phase in ('aim', 'release'):
            for fault in ('disconnect', 'estop', 'stale', 'future', 'overrun', 'latched'):
                with self.subTest(phase=phase, fault=fault):
                    hw = Hardware()
                    d = adr.Dispatcher(hw, lambda a, b: a - b)
                    d.armed = True
                    now = 1000
                    for _ in range(40):
                        d.step(adr.DriveRequest(now, connected=True, grip=adr.GRIP1_CLOSED),
                               False, now, .02)
                        now += 20
                    d.step(adr.DriveRequest(now, connected=True, shoot=True), False, now, .02)
                    now += 20
                    for _ in range(100):
                        if phase != 'release' or d.manipulators.shoot_phase != 'aim':
                            break
                        d.step(adr.DriveRequest(now, connected=True), False, now, .02)
                        now += 20
                    manip = d.manipulators
                    self.assertEqual(phase, manip.shoot_phase)
                    pose, writes = (manip.grip, manip.pitch), len(hw.servos)
                    stamp = now - 201 if fault == 'stale' else now + 1 if fault == 'future' else now
                    r = adr.DriveRequest(stamp, connected=fault != 'disconnect',
                                         estop=fault == 'estop', shoot=True)
                    d.step(r, fault == 'latched', now, .151 if fault == 'overrun' else .02)
                    self.assertFalse(d.armed)
                    self.assertIsNone(manip.shoot_phase)
                    self.assertEqual((0, 0), hw.power)
                    self.assertEqual(writes, len(hw.servos))
                    # Fresh release/Start must not resume the cancelled sequence.
                    for arm in (False, True, False):
                        now += 20
                        d.step(adr.DriveRequest(now, connected=True, arm=arm), False, now, .02)
                    self.assertTrue(d.armed)
                    self.assertEqual(pose, (manip.grip, manip.pitch))
                    self.assertEqual(writes, len(hw.servos))

    def test_shoot_or_manual_movement_blocks_arming(self):
        for values in ({'shoot': True}, {'grip': adr.GRIP1_CLOSED}, {'pitch': 1}):
            with self.subTest(values=values):
                hw = Hardware()
                d = adr.Dispatcher(hw, lambda a, b: a - b)
                d.step(adr.DriveRequest(1000, connected=True), False, 1000, .02)
                d.step(adr.DriveRequest(1020, connected=True, arm=True, **values), False, 1020, .02)
                self.assertFalse(d.armed)
                self.assertEqual([], hw.servos)

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
    def test_vendor_calls_and_conflicting_buttons(self):
        calls, keys, queried = [], set(), []
        mbot = SimpleNamespace(drive_speed=lambda left, right: calls.append(('wheel', left, right)),
            motor_set=lambda power, port: calls.append(('dc', power, port)),
            servo_set=lambda angle, port: calls.append(('servo', angle, port)))
        pad = SimpleNamespace(is_online=lambda: True, is_key_pressed=lambda key: queried.append(key) or key in keys,
                              get_joystick=lambda axis: 0)
        cpi = SimpleNamespace(controller=SimpleNamespace(is_press=lambda key: False))
        hw = adr.CyberPiHardware(cpi, pad, mbot)
        hw.servo(30, adr.GRIP1_PORT)
        hw.drive(10, 20)
        hw.stop()
        self.assertIn(('servo', 30, 's1'), calls)
        self.assertIn(('wheel', 10, -20), calls)
        self.assertIn(('wheel', 0, 0), calls)
        self.assertFalse(hasattr(hw, 'crank'))
        self.assertTrue(all(call[1] == 0 for call in calls if call[0] == 'dc'))
        self.assertEqual(('dc', 0, 'm1'), calls[-1])
        hw.drive(150, -150)
        self.assertEqual(('wheel', 100, 100), calls[-1])
        # RWD: Ly controls only E1 (first argument); Ry only E2 (second).
        hw.drive(*adr.wheel_targets(100, 0, 0))
        self.assertEqual(('wheel', 100, 0), calls[-1])
        hw.drive(*adr.wheel_targets(0, 100, 0))
        self.assertEqual(('wheel', 0, -100), calls[-1])
        with patch.object(adr.time, 'ticks_ms', lambda: 1000, create=True):
            keys.update(('L1', 'L2', 'Up', 'Down', 'N1', 'N2', 'N3', 'N4'))
            r = hw.sample()
            self.assertEqual((0, 0, None, None, True),
                             (r.crank, r.power_adjust, r.grip, r.pitch, r.shoot))
            keys.difference_update(('L1', 'Up', 'N2', 'N3'))
            r = hw.sample()
            self.assertEqual((0, 0, 30, -1, False),
                             (r.crank, r.power_adjust, r.grip, r.pitch, r.shoot))
            keys.clear()
            keys.update(('N2', 'N3'))
            r = hw.sample()
            self.assertEqual((90, 1), (r.grip, r.pitch))
            keys.clear()
            r = hw.sample()
            self.assertEqual((None, None, False), (r.grip, r.pitch, r.shoot))
            self.assertIn(adr.SHOOT_KEY, queried)
            self.assertTrue(set(queried).isdisjoint({"L2", "Up", "Down", "R2"}))

    def test_native_pid_targets_are_bounded_without_host_slew(self):
        self.step(left=20, right=-20)
        self.assertEqual((20, -20), self.hw.power)
        self.step(left=-20, right=20)
        self.assertEqual((-20, 20), self.hw.power)
        self.step(left=100, right=-100)
        self.assertEqual((100, -100), self.hw.power)
        self.step(left=200, right=-200)
        self.assertEqual((100, -100), self.hw.power)
        self.step()
        self.assertEqual((0, 0), self.hw.power)

    def test_bypass_selects_power_api_and_uses_independent_power_limit(self):
        calls = []
        # No speed API: bypass mode must not require or call it, even to stop.
        mbot = SimpleNamespace(drive_power=lambda *args: calls.append(('power', args)),
                               motor_set=lambda *args: calls.append(('dc', args)),
                               servo_set=lambda *args: None)
        with patch.object(adr, 'PID_BYPASS', True), patch.object(
                adr, 'MAX_DRIVE_POWER', 65), patch.object(adr, 'MAX_RPM', 27):
            hw = adr.CyberPiHardware(SimpleNamespace(), None, mbot)
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



class GamepadLinkTest(unittest.TestCase):
    def setUp(self):
        self.online = False
        self.pad = SimpleNamespace(is_online=lambda: self.online,
            is_connected=lambda: True, get_joystick=lambda name: 0,
            is_key_pressed=lambda name: False)
        motors = SimpleNamespace(drive_speed=lambda *args: None, motor_set=lambda *args: None,
            servo_set=lambda *args: None)
        cpi = SimpleNamespace(controller=SimpleNamespace(is_press=lambda key: False))
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

    def test_control_deadline_still_disarms_with_fresh_input(self):
        self.controller.armed = True
        self.step(adr.DriveRequest(1000, 30, 30, True, True), dt=(adr.CONTROL_GAP_MS + 1) / 1000)
        self.assertEqual('control overrun', self.controller.reason)
        self.assertFalse(self.controller.armed)
        self.assertEqual((0, 0), self.hw.power)

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
            self.assertIn('Sticks: drive', frames[-1][0])
            self.assertNotIn('R2', frames[-1][0])
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

    def test_field_tool_runtime_is_commented_out(self):
        for name in ('AdlFieldTool', 'CyberPiBluetooth', 'OptionalFieldTool', 'prepare_field_tool'):
            self.assertFalse(hasattr(adr, name), name)

#     def test_disabled_does_not_create_transport_or_thread(self):
#         self.assertFalse(adr.FIELD_TOOL_ENABLED)
#         with patch.object(adr, 'OptionalFieldTool', side_effect=AssertionError('optional allocation')):
#             self.assertIsNone(adr.prepare_field_tool(self.logger))
#
#     def test_optional_allocation_and_worker_start_failure_are_nonfatal(self):
#         with patch.object(adr, 'FIELD_TOOL_ENABLED', True), patch.object(
#                 adr, 'OptionalFieldTool', side_effect=MemoryError('no heap')):
#             self.assertIsNone(adr.prepare_field_tool(self.logger))
#         service = adr.OptionalFieldTool(self.logger)
#         with patch.object(adr._thread, 'start_new_thread', side_effect=RuntimeError('no thread')):
#             service.start()
#         self.assertTrue(service.failed)
#         self.assertIsNone(service.sample())
#
#     def test_initialization_failure_leaves_local_input_available(self):
#         service = adr.OptionalFieldTool(self.logger)
#         with patch.object(adr, 'CyberPiBluetooth', side_effect=RuntimeError('no communication API')):
#             service.run()
#         self.assertTrue(service.failed)
#         self.assertIsNone(service.sample())
#
#     def test_transport_failure_stops_remote_before_return_to_local_input(self):
#         service = adr.OptionalFieldTool(self.logger)
#         service.fieldtool.exchange(adr.adl_seal('1|1|1'))
#         class BrokenTransport:
#             def __init__(self, *args): pass
#             def pump(self): raise OSError('BLE send failed')
#             def close(self): raise OSError('BLE close failed')
#         with patch.object(adr, 'CyberPiBluetooth', BrokenTransport):
#             service.run()
#         self.assertTrue(service.sample().estop)
#         self.assertIsNone(service.sample())
#
#     def test_stuck_native_io_does_not_block_mailbox_control_or_shutdown(self):
#         service = adr.OptionalFieldTool(self.logger)
#         entered, release = threading.Event(), threading.Event()
#         class StuckTransport:
#             def __init__(self, *args):
#                 self.transport_lock = args[0]
#             def pump(self):
#                 with self.transport_lock:
#                     entered.set()
#                     if not release.wait(2): raise RuntimeError('test release timed out')
#             def close(self): pass
#         with patch.object(adr, 'CyberPiBluetooth', StuckTransport):
#             worker = threading.Thread(target=service.run, daemon=True)
#             worker.start()
#             try:
#                 self.assertTrue(entered.wait(1))
#                 hardware = Hardware()
#                 controller = adr.Dispatcher(hardware, lambda a, b: a - b)
#                 # Local control can arm and drive while native BLE holds its lock.
#                 self.assertIsNone(service.sample())
#                 controller.step(adr.DriveRequest(1000, connected=True), False, 1000, .02)
#                 controller.step(adr.DriveRequest(1000, connected=True, arm=True), False, 1000, .02)
#                 controller.step(adr.DriveRequest(1000, 30, 30, True, True), False, 1000, .02)
#                 self.assertGreater(hardware.power[0], 0)
#                 # The same stall cannot refresh a prior remote motion request.
#                 service.fieldtool.remote = True
#                 service.fieldtool.request = adr.DriveRequest(1000, 30, 30, True, True)
#                 controller.step(service.sample(), False, 1201, .02)
#                 self.assertEqual((0, 0), hardware.power)
#                 self.assertFalse(controller.armed)
#                 service.close()
#                 self.assertTrue(worker.is_alive())  # close never waits on native I/O
#             finally:
#                 release.set()
#                 worker.join(1)
#             self.assertFalse(worker.is_alive())
#
    def test_boot_marker_works_without_lcd_and_tolerates_console_errors(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            adr.boot_mark('03 importing cyberpi')
        self.assertIn('[ADL boot] 03 importing cyberpi', output.getvalue())
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
                                    'motor_set': zero, 'servo_set': lambda *args: None}))
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
        cpi = SimpleNamespace(display=SimpleNamespace(show_label=lambda *args, **kwargs: None))
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
                               servo_set=lambda *args: None)
        cpi = SimpleNamespace(display=SimpleNamespace(show_label=lambda *args, **kwargs: None))
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

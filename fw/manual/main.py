"""CyberPi upload-mode application. Upload this one file, not an ESP32 image.

Main thread: USB/LCD diagnostics and shutdown supervision.
Control worker: gamepad -> wheel speed/power + gripper/pitch servos.
Catapult and field-tool runtime are temporarily commented out.
See docs/fw_adr_architecture.md for wiring and controls. No third-party packages.
"""
def boot_mark(message):
    """Host/USB console breadcrumb, independent of LCD and optional Bluetooth."""
    try:
        print("[ADR boot] " + message)
    except Exception:
        pass


boot_mark("01 file entered; context " + str(globals().get("__name__", "<unset>")) + "; importing time / _thread")
import time
import _thread
import sys
import math
boot_mark("02 runtime imports ready")


# ---------- Configuration: verify wiring, signs and API on your device ----------
DRIVE_MODE = "tank"                 # "tank": Ly/Ry; "arcade": Ly/Rx
PID_BYPASS = False                  # True: raw power (%); False: onboard PID (RPM)
# RWD wiring: E1/EM1 = left rear wheel; E2/EM2 = right rear wheel.
# Both drive APIs address these as first (left) and second (right) arguments.
LEFT_MOTOR_SIGN = 1
RIGHT_MOTOR_SIGN = -1              # Existing mirrored-wheel polarity
LEFT_STICK_SIGN = 1
RIGHT_STICK_SIGN = 1
TURN_STICK_SIGN = -1               # Makeblock Rx: left positive, right negative
AXIS_FULL_SCALE = 100.0
DEADZONE = 0.10
EXPO = 0.35
MAX_RPM = 600 #100.0                    # Full-stick speed; onboard PID regulates motor power
MAX_DRIVE_POWER = 100.0            # Full-stick percent power when PID_BYPASS is True
# SLEW_RPM_PER_SECOND = 150.0      # Optional host speed ramp remains parked
# INPUT_PERIOD_MS = 20             # Field-tool subsystem commented out
CONTROL_PERIOD_MS = 20             # Requested 50 Hz; not a real-time guarantee
CONTROL_STACK_BYTES = 32 * 1024    # Headroom for nested vendor encoder-speed calls
REQUEST_TIMEOUT_MS = 200
CONTROL_GAP_MS = 150

GRIP1_PORT = "s2"                  # Direct-drive beams; no external gearing
GRIP2_PORT = "s3"
GRIP1_NEUTRAL_DEG = 90.0           # Forward-parallel beam pose
GRIP2_NEUTRAL_DEG = 90.0
GRIP_MIN_DEG = 0.0
GRIP_MAX_DEG = 180.0
GRIP1_AXIS_SIGN = 1                # Lx right/inward decreases left beam angle
GRIP2_AXIS_SIGN = -1               # Rx left/inward decreases right beam angle
PITCH_PORT = "s1"                  # Larger positional servo
# Robot frame: 0 = +Z/up, positive rotates toward the front. Servo 0 = -45.
PITCH_MIN_DEG = -45.0
PITCH_MAX_DEG = 135.0
PITCH_HOME_DEG = 0.0
SERVO_STEPS_DEG = (1, 2, 5, 15, 30, 60)
SERVO_STEP_DEFAULT_INDEX = 0
GRIP_BEAM_LENGTH_MM = 170.0
GRIP_BEAM_THICKNESS_MM = 10.0
GRIP_PIVOT_SPACING_MM = 173.5
GRIP_PIVOT_FORWARD_OFFSET_MM = 0.0
GRIP_MIN_CLEARANCE_MM = 10.0
GRIP1_MOUNT_SIGN = 1               # Same-facing shafts rotate in the same sense
GRIP2_MOUNT_SIGN = 1
# Fixed MECDS-150 manufacturer profile; never accepted as calibration inputs.
PITCH_MOTOR_MODEL = "MECDS-150"
PITCH_SERVO_VOLTAGE_V = 6.0
PITCH_STALL_TORQUE_KGFCM = 16.5
PITCH_STALL_TORQUE_NM = 1.61809725
PITCH_NOLOAD_SEC_PER_60DEG = 0.19
PITCH_NOLOAD_SPEED_RAD_S = 5.5116
PITCH_HARDWARE_PROFILE = (PITCH_MOTOR_MODEL, PITCH_SERVO_VOLTAGE_V,
    PITCH_STALL_TORQUE_KGFCM, PITCH_STALL_TORQUE_NM,
    PITCH_NOLOAD_SEC_PER_60DEG, PITCH_NOLOAD_SPEED_RAD_S)
# Mechanical assumptions, separate from manufacturer ratings. +Z = pitch zero.
LAUNCH_MECHANICS = {"arm_length_m": GRIP_BEAM_LENGTH_MM / 1000,
    "arm_mass_kg": 0.025, "ball_mass_kg": 0.015, "start_deg": -35.0,
    "overtravel_deg": None, "travel_max_deg": None}
# Populate from experiments with this arm/ball/voltage. None inhibits N1 shots.
# Speed/acceleration are empirical fits, never inferred from stall/no-load specs.
LAUNCH_CALIBRATION = {"home_duration_us": None, "response_delay_us": None,
    "loaded_speed_rad_s": None, "loaded_accel_rad_s2": None,
    "unloaded_speed_rad_s": None, "unloaded_accel_rad_s2": None,
    "grip_latency_us": None, "ball_detach_delay_us": None,
    "braking_distance_deg": None, "braking_duration_us": None,
    "release_tolerance_deg": None, "friction_nm": None}
LAUNCH_RELEASE_DEG = (90, 90)       # Independent beam/latch release targets
LAUNCH_SERVICE_MS = 1              # Requested supervision cadence, not guaranteed
SHOOT_KEY = "N1"
RELEASE_MODE_KEY = "N2"           # Vertical 0 / last manually commanded pitch
CRANKER_PORT = "m1"                 # Zero-output safety writes only while disabled
# CRANKER_POWER = 100                # Initial magnitude, percent
# CRANKER_POWER_STEP = 10
# CRANKER_POWER_SIGN = 1
GAMEPAD_LINK_METHOD = "is_online"  # CyberOS gamepad.py; unavailable status keeps local input waiting
ARM_KEY = "Start"
ESTOP_KEY = "Select"
PITCH_UP_KEY = "L1"
PITCH_DOWN_KEY = "R1"
SERVO_STEP_PREVIOUS_KEY = "L2"
SERVO_STEP_NEXT_KEY = "R2"
# DEADMAN_KEY = "R2"               # Former drive deadman; now step-size selection
# CRANKER_KEY = "L1"
# RETRACT_KEY = "L2"
# POWER_UP_KEY = "Up"
# POWER_DOWN_KEY = "Down"
# FIELD_TOOL_ENABLED = False        # Optional raw BLE; gamepad works without it
LOG_SERIAL_ENABLED = True         # Structured tracing through built-in print / USB UART
LOG_SCREEN_ENABLED = True
LOG_SCREEN_FONT = 12
GAMEPAD_WAIT_FONT = 16
LOG_SCREEN_COLUMNS = 24
LOG_SCREEN_ROWS = 7
LOG_FLUSH_MS = 250                 # Main-thread diagnostics, outside control worker
LOG_QUEUE_SIZE = 32
LOG_BATCH_SIZE = 1                 # One runtime trace record per flush
app_state = None                   # Read-only USB diagnostics; worker owns updates


# BEGIN GENERATED ADL CODEC
# Generated by fw/manual/field_tool_provider/tools/generate_codec.py. DO NOT EDIT.
ADL_BOUNDS = {1: [], 2: [(-60, 60), (-60, 60), (0, 1)], 3: [], 4: [(1, 2), (0, 180)], 5: [(0, 1)], 6: [], 7: []}


def adl_crc(text):
    crc = 0xffff
    for ch in text:
        if ord(ch) > 127:
            raise ValueError("non ASCII packet")
        crc ^= ord(ch) << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021 if crc & 0x8000 else crc << 1) & 0xffff
    return crc


def adl_seal(body):
    return body + "|%04x" % adl_crc(body)


def adl_decode(wire):
    if not isinstance(wire, str) or len(wire) > 160:
        raise ValueError("packet size/type")
    at = wire.rfind("|")
    if at < 0 or len(wire) - at != 5 or any(c not in "0123456789abcdef" for c in wire[at + 1:]):
        raise ValueError("packet checksum")
    body = wire[:at]
    if adl_crc(body) != int(wire[at + 1:], 16):
        raise ValueError("packet checksum")
    fields = body.split("|")
    if len(fields) < 3 or fields[0] != "1":
        raise ValueError("packet version")
    for s in fields[1:]:
        if not s or len(s) > 11 or (s[0] == "-" and (len(s) == 1 or not s[1:].isdigit())) or (s[0] != "-" and not s.isdigit()):
            raise ValueError("packet integer")
    values = [int(s) for s in fields[1:]]
    ident, op = values[:2]
    bounds = ADL_BOUNDS.get(op)
    if ident < 0 or ident > 2147483647 or bounds is None or len(values) != len(bounds) + 2:
        raise ValueError("packet id/operation/length")
    for n, (low, high) in zip(values[2:], bounds):
        if n < low or n > high:
            raise ValueError("packet value range")
    return ident, op, values[2:]
# END GENERATED ADL CODEC


class KernelLog:
    """Bounded records; main owns USB/LCD I/O, control only publishes records.

    Never hold the actuator/input lock during console calls. Native VM scheduling
    remains device-dependent; the input/control deadlines still apply.
    """
    def __init__(self, clock, ticks_diff):
        self.clock = clock
        self.diff = ticks_diff
        self.lock = _thread.allocate_lock()
        self.last_tick = clock()
        self.elapsed = 0
        self.pending = []
        self.dropped = 0
        self.screen = None
        self.rows = []
        self.dirty = False
        self.last_flush = None
        self.waiting_gamepad = False
        self.screen_mode = None
        self.servo_step = SERVO_STEPS_DEG[SERVO_STEP_DEFAULT_INDEX]
        self.screen_step = None
        self.launcher_status = ("IDLE", True)
        self.screen_launcher = None

    def emit(self, level, tag, message):
        message = str(message).replace("\r", " ").replace("\n", " ")[:160]
        self.lock.acquire()
        try:
            tick = self.clock()
            self.elapsed += max(0, self.diff(tick, self.last_tick))
            self.last_tick = tick
            if len(self.pending) >= LOG_QUEUE_SIZE:
                self.pending.pop(0)
                self.dropped += 1
            self.pending.append((self.elapsed, level, tag, message))
        finally:
            self.lock.release()

    def attach_screen(self, cyberpi):
        if not LOG_SCREEN_ENABLED:
            return
        try:
            self.screen = cyberpi.display
            if not callable(getattr(self.screen, "show_label", None)):
                raise RuntimeError("Missing display.show_label")
        except Exception as exc:
            self.screen = None
            boot_mark("LCD unavailable: " + str(exc))
            self.emit("ERROR", "lcd", "display unavailable: " + str(exc))
            return
        # Optional initial styling is complete before the control worker starts.
        for fn, args in ((getattr(self.screen, "clear", None), ()),
                         (getattr(self.screen, "set_brush", None), (255, 255, 255))):
            if callable(fn):
                try:
                    fn(*args)
                except Exception as exc:
                    self.emit("WARN", "lcd", "style unavailable: " + str(exc))

    def _append_screen(self, stamp, level, tag, message):
        text = "[%6.3f] %s %s: %s" % (stamp / 1000.0, level[0], tag, message)
        width = LOG_SCREEN_COLUMNS
        while text:
            self.rows.append(text[:width])
            text = "  " + text[width:] if len(text) > width else ""
        self.rows = self.rows[-LOG_SCREEN_ROWS:]
        self.dirty = True

    def set_gamepad_waiting(self, waiting):
        """Publish display state without calling the console from input polling."""
        if self.waiting_gamepad == waiting:
            return
        self.lock.acquire()
        try:
            self.waiting_gamepad = waiting
        finally:
            self.lock.release()

    def set_servo_step(self, degrees):
        """Control publishes selection; main remains the only LCD owner."""
        self.lock.acquire()
        try:
            self.servo_step = degrees
        finally:
            self.lock.release()

    def set_launcher(self, state, vertical):
        self.lock.acquire()
        try:
            self.launcher_status = (state, vertical)
        finally:
            self.lock.release()

    def pump(self, force=False):
        """Main thread only; never called from input/control or transport."""
        tick = self.clock()
        if not force and self.last_flush is not None and self.diff(tick, self.last_flush) < LOG_FLUSH_MS:
            return
        self.last_flush = tick
        self.lock.acquire()
        try:
            count = len(self.pending) if force else LOG_BATCH_SIZE
            records, dropped = self.pending[:count], self.dropped
            self.pending, self.dropped = self.pending[count:], 0
            waiting = self.waiting_gamepad
            servo_step = self.servo_step
            launcher = self.launcher_status
        finally:
            self.lock.release()
        if dropped:
            stamp = records[0][0] if records else self.elapsed
            records.insert(0, (stamp, "WARN", "log", "%d older records dropped" % dropped))
        for stamp, level, tag, message in records:
            if LOG_SERIAL_ENABLED:
                try:
                    print("[ADL %6.3f]" % (stamp / 1000.0), level, tag + ":", message)
                except Exception:
                    pass
            self._append_screen(stamp, level, tag, message)
        mode = "waiting" if waiting else ("log" if force else "ready")
        step_changed = mode == "ready" and (servo_step != self.screen_step or launcher != self.screen_launcher)
        if self.screen is None or (not self.dirty and self.screen_mode == mode and not step_changed):
            return
        if mode in ("waiting", "ready") and self.screen_mode == mode and not step_changed:
            # CyberOS display calls can stall the VM. Runtime logs remain on USB;
            # Repaint only on link/step selection changes, never for log traffic.
            self.dirty = False
            return
        try:
            # Match the working sample: replace one indexed label in one call.
            text = ("WAITING\nFOR\nGAMEPAD" if waiting else
                    "GAMEPAD READY\nStep: %d deg L2-/R2+\nN1:shoot N2:%s\nShot: %s\nLy/Ry: drive\nLx/Rx: beams\nL1/R1: pitch\nStart:arm Select/B:stop" %
                    (servo_step, "0deg" if launcher[1] else "manual", launcher[0]) if mode == "ready" else
                    "\n".join(self.rows))
            self.screen.show_label(text,
                                   GAMEPAD_WAIT_FONT if waiting else LOG_SCREEN_FONT,
                                   0, 0, index=0)
            self.screen_mode = mode
            self.screen_step = servo_step
            self.screen_launcher = launcher
            self.dirty = False
        except Exception:
            self.screen = None   # Display failure must not interrupt control


def clamp(value, low, high):
    return max(low, min(high, value))


def number(value, limit):
    """Reject None, bool, NaN, infinity and implausible values; never fake zero."""
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError("non-numeric hardware value")
    if value != value or abs(value) > limit:
        raise ValueError("hardware value out of range")
    return float(value)


def axis(raw):
    x = clamp(number(raw, AXIS_FULL_SCALE * 1.1) / AXIS_FULL_SCALE, -1.0, 1.0)
    magnitude = abs(x)
    if magnitude <= DEADZONE:
        return 0.0
    x = (magnitude - DEADZONE) / (1.0 - DEADZONE) * (1 if x >= 0 else -1)
    return (1.0 - EXPO) * x + EXPO * x * x * x


def drive_limit():
    return MAX_DRIVE_POWER if PID_BYPASS else MAX_RPM


def wheel_targets(ly, ry, rx):
    left = axis(ly) * LEFT_STICK_SIGN
    if DRIVE_MODE == "tank":
        right = axis(ry) * RIGHT_STICK_SIGN
    elif DRIVE_MODE == "arcade":
        turn = axis(rx) * TURN_STICK_SIGN
        left, right = left + turn, left - turn
        scale = max(1.0, abs(left), abs(right))
        left, right = left / scale, right / scale
    else:
        raise ValueError("invalid DRIVE_MODE")
    limit = drive_limit()
    return left * limit, right * limit


class DriveRequest:
    """Wheel RPM/power and independent beam/pitch directions."""
    def __init__(self, stamp, left=0.0, right=0.0, connected=False,
                 deadman=False, arm=False, estop=False,
                 grip_left=None, grip_right=None, pitch=None, crank=0, power_adjust=0,
                 step_previous=False, step_next=False, shoot=False, release_mode=False):
        self.stamp = stamp
        self.left = left
        self.right = right
        self.connected = connected
        self.deadman = deadman    # Legacy field-tool slot; ignored by local control
        self.arm = arm
        self.estop = estop
        self.grip_left = grip_left
        self.grip_right = grip_right
        self.pitch = pitch
        self.crank = crank
        self.power_adjust = power_adjust
        self.step_previous = step_previous
        self.step_next = step_next
        self.shoot = shoot
        self.release_mode = release_mode


class WorkerState:
    """One-way shutdown/completion flags; no request handoff or hardware lock.

    Main and worker may only set shutdown true. Only the control worker publishes
    completion, after its final stop attempts. Field-tool runtime is commented out.
    """
    def __init__(self):
        self.shutdown = False
        self.worker_done = False
        self.armed = False
        self.reason = "boot"
        self.cycles = 0
        self.max_cycle_ms = 0
        self.timing_faults = 0
        self.launch_state = "IDLE"
        self.launch_reason = ""
        self.failure = None         # Preserve traceback for main, after outputs stop
        self.failure_stage = None

    def close(self):
        self.shutdown = True

    def done(self):
        self.worker_done = True

    def is_done(self):
        return self.worker_done


# TEMPORARILY DISABLED: catapult controller; retain source for restoration.
# class Cranker:
#     """Direct hold-to-run output; one power step per directional-key press."""
#     def __init__(self):
#         self.power = CRANKER_POWER
#         self.adjust_last = 0
#
#     def cancel(self):
#         # Retain selected power across deadman release and re-arming.
#         self.adjust_last = 0
#
#     def update(self, direction, adjustment):
#         if adjustment and adjustment != self.adjust_last:
#             self.power = clamp(self.power + adjustment * CRANKER_POWER_STEP, 0, 100)
#         self.adjust_last = adjustment
#         return direction * self.power * CRANKER_POWER_SIGN
#
#
class CyberPiHardware:
    """Gamepad and actuators have one owner: the control worker, after boot.

    Both drive APIs control E1/EM1 left rear and E2/EM2 right rear, respectively.
    PID_BYPASS selects raw power instead of onboard encoder speed regulation.
    """
    def __init__(self, cyberpi, gamepad, mbot2):
        self.cpi = cyberpi
        self.mbot = mbot2
        self.pad = gamepad
        self.link = getattr(gamepad, GAMEPAD_LINK_METHOD, None)
        self.gamepad_wait_reason = ("gamepad not connected" if callable(self.link)
                                    else "gamepad link API unavailable")
        drive_api = "drive_power" if PID_BYPASS else "drive_speed"
        for name in (drive_api, "motor_set"):
            if not callable(getattr(self.mbot, name, None)):
                raise RuntimeError("Missing mbot2 API: " + name)
        self.wheel_drive = getattr(self.mbot, drive_api)
        self.servo_api = getattr(self.cpi, "mbot2", None)
        for name in ("servo_set", "servo_add"):
            if not callable(getattr(self.servo_api, name, None)):
                raise RuntimeError("Missing cpi.mbot2 API: " + name)

    def _gamepad_connected(self):
        """Control-worker only; never infer connectivity from axis values."""
        self.link = getattr(self.pad, GAMEPAD_LINK_METHOD, None)
        if not callable(self.link):
            self.gamepad_wait_reason = "gamepad link API unavailable"
            return False
        try:
            link = self.link()
        except Exception as exc:
            self.gamepad_wait_reason = "gamepad link unavailable: " + str(exc)
            return False
        if link not in (True, False, 0, 1):
            self.gamepad_wait_reason = "invalid gamepad link status"
            return False
        if not link:
            self.gamepad_wait_reason = "gamepad not connected"
            return False
        for name in ("get_joystick", "is_key_pressed"):
            if not callable(getattr(self.pad, name, None)):
                self.gamepad_wait_reason = "gamepad input API unavailable: " + name
                return False
        self.gamepad_wait_reason = None
        return True

    def sample(self):
        # Timestamp before all reads: native input delay counts toward freshness.
        stamp = time.ticks_ms()
        local_stop = self.cpi.controller.is_press("b")
        if not self._gamepad_connected():
            return DriveRequest(stamp, estop=bool(local_stop))
        key = self.pad.is_key_pressed
        estop = local_stop or key(ESTOP_KEY)
        if estop:
            return DriveRequest(stamp, connected=True, estop=True)
        ly = self.pad.get_joystick("Ly")
        ry = self.pad.get_joystick("Ry") if DRIVE_MODE == "tank" else 0
        lx = self.pad.get_joystick("Lx")
        rx = self.pad.get_joystick("Rx")
        left, right = wheel_targets(ly, ry, rx)
        grip_left = axis(lx) * GRIP1_AXIS_SIGN
        grip_right = axis(rx) * GRIP2_AXIS_SIGN
        pitch_raise, pitch_lower = key(PITCH_UP_KEY), key(PITCH_DOWN_KEY)
        pitch = None if pitch_lower == pitch_raise else (1 if pitch_lower else -1)
        # N1 starts the coordinated shot; legacy M1 catapult stays disabled.
        # crank = int(bool(key(CRANKER_KEY))) - int(bool(key(RETRACT_KEY)))
        # adjust = int(bool(key(POWER_UP_KEY))) - int(bool(key(POWER_DOWN_KEY)))
        return DriveRequest(stamp, left, right, connected=True,
                            arm=bool(key(ARM_KEY)), grip_left=grip_left,
                            grip_right=grip_right, pitch=pitch,
                            step_previous=bool(key(SERVO_STEP_PREVIOUS_KEY)),
                            step_next=bool(key(SERVO_STEP_NEXT_KEY)),
                            shoot=bool(key(SHOOT_KEY)), release_mode=bool(key(RELEASE_MODE_KEY)))

    def local_stop(self):
        return bool(self.cpi.controller.is_press("b")) or bool(
            self._gamepad_connected() and self.pad.is_key_pressed(ESTOP_KEY))

    def drive(self, left, right):
        limit = drive_limit()
        left = clamp(number(left, 1000), -limit, limit)
        right = clamp(number(right, 1000), -limit, limit)
        self.wheel_drive(int(round(left * LEFT_MOTOR_SIGN)),
                         int(round(right * RIGHT_MOTOR_SIGN)))

    def servo(self, angle, port):
        if not 0 <= angle <= 180:
            raise ValueError("servo angle outside 0..180")
        self.servo_api.servo_set(int(angle), port)

    def servo_add(self, delta, port):
        delta = int(number(delta, 180))
        if delta:
            self.cpi.mbot2.servo_add(delta, port)

    # Catapult actuation disabled; only stop()/boot can write zero to M1.
    # def crank(self, power):
    #     self.mbot.motor_set(int(power), CRANKER_PORT)

    def stop(self):
        # Attempt M1 zero even if the wheel command fails; preserve first error.
        error = None
        for fn, args in ((self.wheel_drive, (0, 0)),
                         (self.mbot.motor_set, (0, CRANKER_PORT))):
            try:
                fn(*args)
            except Exception as exc:
                if error is None:
                    error = exc
        if error is not None:
            raise error


# TEMPORARILY DISABLED: field-tool mailbox, BLE transport and startup factory.
# class AdlFieldTool:
#     """Bounded BLE mailbox. The API callback only publishes requests; worker owns outputs.
#
#     Native CyberPi BLE bytes are passed here by the optional transport worker.
#     A response confirms admission, not physical motion.
#     """
#     def __init__(self, clock, ticks_diff):
#         self.clock = clock
#         self.diff = ticks_diff
#         self.lock = _thread.allocate_lock()
#         self.remote = False
#         self.request = None
#         self.arm_phase = 0
#         self.armed = False
#         self.stop_pending = False
#         self.seen = []
#
#     def exchange(self, wire):
#         ident, op, values = adl_decode(wire)
#         now = self.clock()
#         self.lock.acquire()
#         try:
#             for old_id, old_wire, response in self.seen:
#                 if ident == old_id:
#                     if wire != old_wire:
#                         raise ValueError("request id reused")
#                     return response       # Never refresh motion age or retrigger a side effect.
#             code = 0
#             if op == 6:                  # Status never renews motion or changes ownership.
#                 pass
#             elif op == 7:                # Explicit return to gamepad; force a worker stop first.
#                 self.remote = False
#                 self.request = None
#                 self.arm_phase = 0
#                 self.stop_pending = True
#             elif op == 3:
#                 self.remote = True
#                 self.request = DriveRequest(now, connected=True, estop=True)
#                 self.arm_phase = 0
#                 self.stop_pending = True
#             elif op == 1:
#                 # Two main-loop samples provide neutral release then fresh Start.
#                 self.remote = True
#                 self.request = DriveRequest(now, connected=True)
#                 self.arm_phase = 2
#             elif not self.remote or not self.armed:
#                 code = 1                # Must deliberately arm before commanding actuators.
#             elif op == 2:
#                 self.arm_phase = 0
#                 self.request = DriveRequest(now, values[0], values[1], True, bool(values[2]))
#             elif op == 4:
#                 self.request = DriveRequest(now, connected=True, deadman=True,
#                     grip=values[1] if values[0] == 1 else None,
#                     pitch=values[1] if values[0] == 2 else None)
#             elif op == 5:
#                 self.request = DriveRequest(now, connected=True, deadman=True, crank=bool(values[0]))
#             else:
#                 code = 2
#             response = adl_seal("1|%d|%d|%d|%d" % (ident, op, code, int(self.armed)))
#             self.seen.append((ident, wire, response))
#             self.seen = self.seen[-32:]
#             return response
#         finally:
#             self.lock.release()
#
#     def sample(self):
#         self.lock.acquire()
#         try:
#             if self.stop_pending:
#                 self.stop_pending = False
#                 return DriveRequest(self.clock(), connected=True, estop=True)
#             if not self.remote:
#                 return None
#             request = self.request
#             if request is None:
#                 return DriveRequest(self.clock(), estop=True)
#             # Keep the original command timestamp. Polling the mailbox cannot keep motion alive.
#             if self.arm_phase and 0 <= self.diff(self.clock(), request.stamp) <= REQUEST_TIMEOUT_MS:
#                 result = DriveRequest(request.stamp, connected=True, arm=self.arm_phase == 1)
#                 self.arm_phase -= 1
#                 return result
#             return request
#         finally:
#             self.lock.release()
#
#     def release(self):
#         """Return to local input after a transport fault; latch a remote stop first."""
#         self.lock.acquire()
#         try:
#             if self.remote:
#                 self.stop_pending = True
#             self.remote = False
#             self.request = None
#             self.arm_phase = 0
#         finally:
#             self.lock.release()
#
#
# class CyberPiBluetooth:
#     """HAL for CyberOS' native communication object; owns only the BLE raw channel.
#
#     cyberpi.communication.disable_channel_default('bluetooth') routes BLE to raw I/O.
#     The public wrapper converts outgoing data for the native bytearray-only send.
#     Native API evidence is retained in field_tool_provider/bluetooth-evidence.md.
#     """
#     def __init__(self, transport_lock, fieldtool, clock, ticks_diff, communication=None):
#         if communication is None:
#             import cyberpi
#             communication = cyberpi.communication
#         self.api = communication
#         self.channel = "bluetooth"
#         for name in ("disable_channel_default", "enable_channel_default", "read", "send"):
#             if not callable(getattr(self.api, name, None)):
#                 raise RuntimeError("Missing CyberPi Bluetooth API: " + name)
#         self.lock = transport_lock
#         self.fieldtool = fieldtool
#         self.clock = clock
#         self.diff = ticks_diff
#         self.buffer = ""
#         self.dropping = False
#         self.last_byte = clock()
#         self.rejected = 0
#         self.lock.acquire()
#         try:
#             self.api.disable_channel_default(self.channel)
#         except Exception:
#             # A partial initialization must not intentionally retain raw ownership.
#             try:
#                 self.api.enable_channel_default(self.channel)
#             except Exception:
#                 pass
#             raise
#         finally:
#             self.lock.release()
#
#     def _read(self):
#         self.lock.acquire()
#         try:
#             return self.api.read(self.channel)
#         finally:
#             self.lock.release()
#
#     def _send(self, text):
#         data = bytearray(text + "\n", "ascii")
#         self.lock.acquire()
#         try:
#             for at in range(0, len(data), 20):
#                 self.api.send(self.channel, data[at:at + 20])
#         finally:
#             self.lock.release()
#
#     def pump(self):
#         now = self.clock()
#         if self.buffer and self.diff(now, self.last_byte) > 500:
#             self.buffer = ""
#             self.dropping = False
#             self.rejected += 1
#         # Native recv_raw returns up to 128 bytes. Bound work per input period.
#         for _ in range(2):
#             data = self._read()
#             if data is None or len(data) == 0:
#                 break
#             if not isinstance(data, (bytes, bytearray)) or len(data) > 128:
#                 raise ValueError("invalid CyberPi Bluetooth read")
#             self.last_byte = now
#             for ch in data:
#                 if ch == 10:
#                     if self.buffer and not self.dropping:
#                         try:
#                             response = self.fieldtool.exchange(self.buffer)
#                         except ValueError:
#                             self.rejected += 1
#                         else:
#                             self._send(response)
#                     self.buffer = ""
#                     self.dropping = False
#                 elif ch < 32 or ch > 126 or len(self.buffer) >= 160:
#                     self.buffer = ""
#                     self.dropping = True
#                 elif not self.dropping:
#                     self.buffer += chr(ch)
#
#     def close(self):
#         self.lock.acquire()
#         try:
#             self.api.enable_channel_default(self.channel)
#         finally:
#             self.lock.release()
#
#
# class OptionalFieldTool:
#     """Isolate native BLE I/O from input/control; never join this optional worker.
#
#     The mailbox lock is never held across native I/O. A stuck read/send leaves
#     remote requests to expire normally and cannot hold the actuator API lock.
#     """
#     def __init__(self, logger):
#         self.logger = logger
#         self.fieldtool = AdlFieldTool(time.ticks_ms, time.ticks_diff)
#         self.running = True
#         self.failed = False
#         self.failure_handled = False
#
#     def start(self):
#         try:
#             _thread.start_new_thread(self.run, ())
#         except Exception as exc:
#             self.failed = True
#             self.logger.emit("WARN", "fieldtool", "worker unavailable: " + str(exc))
#
#     def run(self):
#         bluetooth = None
#         try:
#             # This lock belongs only to the transport, never CyberPiHardware.
#             bluetooth = CyberPiBluetooth(_thread.allocate_lock(), self.fieldtool,
#                                         time.ticks_ms, time.ticks_diff)
#             self.logger.emit("INFO", "fieldtool", "optional BLE worker online")
#             while self.running:
#                 bluetooth.pump()
#                 time.sleep_ms(INPUT_PERIOD_MS)
#         except Exception as exc:
#             self.logger.emit("WARN", "fieldtool", "disabled: " + str(exc))
#         finally:
#             self.failed = True
#             if bluetooth is not None:
#                 try:
#                     bluetooth.close()
#                 except Exception as exc:
#                     self.logger.emit("WARN", "fieldtool", "close failed: " + str(exc))
#
#     def sample(self):
#         if self.failed and not self.failure_handled:
#             self.fieldtool.release()
#             self.failure_handled = True
#         return self.fieldtool.sample()
#
#     def close(self):
#         # Native close may block too; only the optional worker calls it.
#         self.running = False
#
#
# def prepare_field_tool(logger):
#     if not FIELD_TOOL_ENABLED:
#         logger.emit("INFO", "fieldtool", "disabled; local gamepad available")
#         return None
#     try:
#         return OptionalFieldTool(logger)
#     except Exception as exc:
#         logger.emit("WARN", "fieldtool", "unavailable; local gamepad: " + str(exc))
#         return None
#
#
def point_segment_distance_sq(point, start, end):
    dx, dy = end[0] - start[0], end[1] - start[1]
    length_sq = dx * dx + dy * dy
    t = 0 if not length_sq else clamp(((point[0] - start[0]) * dx +
                                       (point[1] - start[1]) * dy) / length_sq, 0, 1)
    x, y = point[0] - start[0] - t * dx, point[1] - start[1] - t * dy
    return x * x + y * y


def segment_distance_sq(a, b, c, d):
    ux, uy, vx, vy = b[0] - a[0], b[1] - a[1], d[0] - c[0], d[1] - c[1]
    cross = ux * vy - uy * vx
    if abs(cross) > 1e-9:
        wx, wy = c[0] - a[0], c[1] - a[1]
        t, u = (wx * vy - wy * vx) / cross, (wx * uy - wy * ux) / cross
        if 0 <= t <= 1 and 0 <= u <= 1:
            return 0.0
    return min(point_segment_distance_sq(a, c, d), point_segment_distance_sq(b, c, d),
               point_segment_distance_sq(c, a, b), point_segment_distance_sq(d, a, b))


class BeamGuard:
    """Conservative swept clearance, using only successfully issued poses.

    Each 170 x 10 mm beam is enclosed by a radius-5 mm capsule with its
    centerline starting at the endpoint-center pivot. +Y is forward, +X right.
    """
    def _segment(self, left, angle):
        pivot = (0.0, 0.0) if left else (GRIP_PIVOT_SPACING_MM, GRIP_PIVOT_FORWARD_OFFSET_MM)
        neutral = GRIP1_NEUTRAL_DEG if left else GRIP2_NEUTRAL_DEG
        sign = GRIP1_MOUNT_SIGN if left else GRIP2_MOUNT_SIGN
        theta = (angle - neutral) * sign * math.pi / 180
        end = (pivot[0] + GRIP_BEAM_LENGTH_MM * math.sin(theta),
               pivot[1] + GRIP_BEAM_LENGTH_MM * math.cos(theta))
        return pivot, end

    def allows(self, left, before, after, other):
        fixed = self._segment(not left, other)
        count = max(1, int(math.ceil(abs(after - before))))
        interval = abs(after - before) * math.pi / 180 / count
        # Nearest samples are at most half an interval away. This displacement
        # bound covers the sweep between samples, including narrow crossings.
        margin = 2 * GRIP_BEAM_LENGTH_MM * math.sin(interval / 4)
        required = GRIP_BEAM_THICKNESS_MM + GRIP_MIN_CLEARANCE_MM + margin
        for i in range(count + 1):
            moving = self._segment(left, before + (after - before) * i / count)
            if segment_distance_sq(moving[0], moving[1], fixed[0], fixed[1]) < required * required - 1e-9:
                return False
        return True


class ServoMotion:
    """One whole servo_add per fresh input; no ramp, feedback read or queue."""
    def __init__(self, hardware, port, home, low, high, offset, logger=None):
        self.hw, self.port = hardware, port
        self.position = home
        self.low, self.high, self.offset = low, high, offset
        self.logger = logger
        self.initialized = False
        self.home_attempted = False
        self.input_ready = False
        self.allow = None

    def cancel(self):
        self.input_ready = False

    def _warn(self, angle, detail):
        if self.logger is not None:
            self.logger.emit("WARN", "servo", "%s: dropped %d deg; %s" %
                             (self.port, angle, detail))

    def _home(self):
        self.home_attempted = True
        angle = int(round(clamp(self.position, self.low, self.high) + self.offset))
        try:
            self.hw.servo(angle, self.port)
        except Exception as exc:
            self._warn(angle, "initial set failed: " + str(exc))
            return
        self.position = angle - self.offset
        self.initialized = True

    def update(self, direction, step):
        if not self.home_attempted:
            self._home()
            self.input_ready = not direction
            return
        if not direction:
            self.input_ready = True
            return
        ready, self.input_ready = self.input_ready, False
        if not ready:
            return  # Held or reversed inputs never accumulate additional moves.
        if not self.initialized:
            self._home()  # An explicit new input may retry failed initialization.
            return
        self.command(self.position + (step if direction > 0 else -step))

    def command(self, target):
        """Issue one bounded target; result describes dispatch, never arrival."""
        if not self.initialized:
            return False
        target = clamp(target, self.low, self.high)
        angle = int(round(target + self.offset))
        target = angle - self.offset
        delta = angle - int(round(self.position + self.offset))
        if not delta:
            return True
        if self.allow is not None and not self.allow(target):
            self._warn(angle, "beam sweep cannot ensure %g mm clearance" % GRIP_MIN_CLEARANCE_MM)
            return False
        try:
            self.hw.servo_add(delta, self.port)
        except Exception as exc:
            self._warn(angle, "add failed: " + str(exc))
            return False
        self.position = target
        return True


class Manipulators:
    """Independent atomic moves, shared step selection and beam clearance guard."""
    def __init__(self, hardware, ticks_diff, logger=None):
        self.logger = logger
        self.step_index = SERVO_STEP_DEFAULT_INDEX
        self.step_released = False
        self.guard = BeamGuard()
        self.grip_left = ServoMotion(hardware, GRIP1_PORT, GRIP1_NEUTRAL_DEG,
                                     GRIP_MIN_DEG, GRIP_MAX_DEG, 0, logger)
        self.grip_right = ServoMotion(hardware, GRIP2_PORT, GRIP2_NEUTRAL_DEG,
                                      GRIP_MIN_DEG, GRIP_MAX_DEG, 0, logger)
        self.pitch = ServoMotion(hardware, PITCH_PORT, PITCH_HOME_DEG,
                                 PITCH_MIN_DEG, PITCH_MAX_DEG, -PITCH_MIN_DEG, logger)
        self.grip_left.allow = self._left_allowed
        self.grip_right.allow = self._right_allowed
        self.servos = (self.grip_left, self.grip_right, self.pitch)
        self.last_manual_pitch = PITCH_HOME_DEG

    def _left_allowed(self, target):
        return self.grip_right.initialized and self.guard.allows(
            True, self.grip_left.position, target, self.grip_right.position)

    def _right_allowed(self, target):
        return self.grip_left.initialized and self.guard.allows(
            False, self.grip_right.position, target, self.grip_left.position)

    def select_step(self, previous, next_step):
        if not previous and not next_step:
            self.step_released = True
            return
        released, self.step_released = self.step_released, False
        if not released or previous == next_step:
            return
        self.step_index = (self.step_index + (1 if next_step else -1)) % len(SERVO_STEPS_DEG)
        step = SERVO_STEPS_DEG[self.step_index]
        if self.logger is not None:
            publish = getattr(self.logger, "set_servo_step", None)
            if callable(publish):
                publish(step)
            self.logger.emit("INFO", "servo", "step %d deg" % step)

    def cancel(self):
        for servo in self.servos:
            servo.cancel()

    def update(self, grip_left, grip_right, pitch, now, dt):
        step = SERVO_STEPS_DEG[self.step_index]
        self.grip_left.update(grip_left, step)
        self.grip_right.update(grip_right, step)
        before = self.pitch.position
        self.pitch.update(pitch, step)
        if self.pitch.position != before:
            self.last_manual_pitch = self.pitch.position

    def release_allowed(self):
        left, right = LAUNCH_RELEASE_DEG
        return (all(servo.initialized for servo in self.servos) and
                self.guard.allows(True, self.grip_left.position, left, self.grip_right.position) and
                self.guard.allows(False, self.grip_right.position, right, left))


def motion_distance(seconds, acceleration, speed, initial=0.0):
    """Empirical constant-acceleration/speed-cap fit, SI units throughout."""
    if not acceleration:
        return initial * seconds
    ramp = min(seconds, (speed - initial) / acceleration)
    return initial * ramp + acceleration * ramp * ramp / 2 + speed * (seconds - ramp)


def motion_time(distance, acceleration, speed, initial=0.0):
    if distance <= 0:
        return 0.0
    if not acceleration:
        return distance / initial
    ramp = (speed - initial) / acceleration
    ramp_distance = (initial + speed) * ramp / 2
    if distance <= ramp_distance:
        return 2 * distance / (math.sqrt(initial * initial + 2 * acceleration * distance) + initial)
    return ramp + (distance - ramp_distance) / speed


class LaunchModel:
    """Design mechanics and calibrated motion; ratings never drive this fit."""
    def __init__(self, mechanics=None, calibration=None):
        self.mechanics = dict(LAUNCH_MECHANICS if mechanics is None else mechanics)
        self.calibration = dict(LAUNCH_CALIBRATION if calibration is None else calibration)
        self.hardware_profile = PITCH_HARDWARE_PROFILE  # Immutable tuple, no overrides

    def inertia(self, loaded=True):
        m = self.mechanics
        return (m["arm_mass_kg"] / 3 + (m["ball_mass_kg"] if loaded else 0)) * m["arm_length_m"] ** 2

    def net_acceleration(self, motor_torque_nm, angle_rad, loaded=True):
        """Theoretical diagnostic only; caller must supply operating torque."""
        m, friction = self.mechanics, self.calibration.get("friction_nm")
        if friction is None:
            raise ValueError("friction_nm requires calibration")
        # From +Z, gravity opposes the forward stroke before vertical and aids
        # it after vertical. This is the signed opposing term in tau - gravity.
        gravity = -(m["arm_mass_kg"] / 2 + (m["ball_mass_kg"] if loaded else 0)) * 9.80665 * m["arm_length_m"] * math.sin(angle_rad)
        return (motor_torque_nm - gravity - friction) / self.inertia(loaded)

    def peak_zero_speed_reference(self, angle_rad, loaded=True):
        # A theoretical peak reference, never a trajectory acceleration command.
        return self.net_acceleration(PITCH_STALL_TORQUE_NM, angle_rad, loaded)

    def plan(self, release_deg):
        m, c = self.mechanics, self.calibration
        positive = ("loaded_speed_rad_s", "loaded_accel_rad_s2", "unloaded_speed_rad_s",
                    "home_duration_us", "braking_distance_deg", "braking_duration_us", "release_tolerance_deg")
        for key in ("arm_length_m", "arm_mass_kg", "ball_mass_kg", "start_deg", "overtravel_deg", "travel_max_deg"):
            value = m.get(key)
            if value is None or isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
                raise ValueError("mechanics required: " + key)
        for key in LAUNCH_CALIBRATION:
            if key == "friction_nm":
                continue  # Empirical fits already include friction; theory alone needs it.
            value = c.get(key)
            if value is None or isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
                raise ValueError("calibration required: " + key)
            if value < 0 or (key in positive and value == 0):
                raise ValueError("invalid calibration: " + key)
        if any(m[key] <= 0 for key in ("arm_length_m", "arm_mass_kg", "ball_mass_kg")):
            raise ValueError("invalid arm/ball model")
        start = int(round(m["start_deg"]))
        target = int(round(release_deg + m["overtravel_deg"]))
        brake = target - c["braking_distance_deg"]
        if not PITCH_MIN_DEG <= start < release_deg < brake < target < min(PITCH_MAX_DEG, m["travel_max_deg"]):
            raise ValueError("release/overtravel/stopping distance outside travel envelope")
        cross = motion_time((release_deg - start) * math.pi / 180,
                            c["loaded_accel_rad_s2"], c["loaded_speed_rad_s"])
        latency = c["grip_latency_us"] + c["ball_detach_delay_us"]
        trigger = int(round(cross * 1000000 - latency))
        if trigger < 0:
            raise ValueError("insufficient acceleration travel for release latency")
        omega = min(c["loaded_speed_rad_s"], c["loaded_accel_rad_s2"] * cross)
        if c["unloaded_speed_rad_s"] < omega:
            raise ValueError("unloaded fit would intentionally decelerate at release")
        # All shot-relative deadlines must fit well inside ticks_diff's half-period.
        post = motion_time((brake - release_deg) * math.pi / 180,
                           c["unloaded_accel_rad_s2"], c["unloaded_speed_rad_s"], omega)
        total = c["home_duration_us"] + c["response_delay_us"] + (cross + post) * 1000000 + c["braking_duration_us"]
        if total > 10000000:
            raise ValueError("shot estimate exceeds 10 s scheduling horizon")
        return {"start_deg": start, "release_deg": release_deg, "target_deg": target,
                "brake_deg": brake, "trigger_us": trigger, "latency_us": latency,
                "trigger_deg": start + motion_distance(trigger / 1000000,
                    c["loaded_accel_rad_s2"], c["loaded_speed_rad_s"]) * 180 / math.pi}


class Launcher:
    """Single user-triggered sequence. All physical states/times are ESTIMATED.

    No servo reads, waits for arrival, retries or intermediate pitch targets.
    Polling and both actuators use the same wrap-safe microsecond timebase.
    """
    ACTIVE = ("HOMING", "LOAD", "ARMED", "ACCELERATING", "RELEASE_PENDING",
              "RELEASED", "OVERRUN", "DECELERATING")

    def __init__(self, manipulators, logger=None, clock=None, diff=None, model=None):
        self.manip = manipulators
        self.logger = logger
        self.clock = clock if clock is not None else getattr(time, "ticks_us", None)
        self.diff = diff if diff is not None else getattr(time, "ticks_diff", None)
        self.model = model if model is not None else LaunchModel()
        self.state = "IDLE"
        self.reason = ""
        self.vertical = True
        self.shoot_released = self.mode_released = False
        self.events = []
        self.epoch = None
        self.elapsed_us = 0
        self.release_commands_us = None
        self.latch_open_est_us = None
        self.ball_detach_est_us = None
        self.current_inertia = None  # Validate mechanics only when N1 is requested.
        self.plan = None

    def active(self):
        return self.state in self.ACTIVE

    def _elapsed(self):
        elapsed = self.diff(self.clock(), self.epoch)
        if elapsed < self.elapsed_us:
            raise ValueError("launch microsecond clock moved backwards / horizon exceeded")
        self.elapsed_us = elapsed
        return elapsed

    def _state(self, state, detail="estimated"):
        self.state = state
        self.events.append((state, self.elapsed_us))
        self.events = self.events[-16:]
        if self.logger is not None:
            self.logger.emit("WARN" if state in ("FAULT", "INHIBITED") else "INFO", "launch",
                             "%s t=%d us; %s" % (state, self.elapsed_us, detail))
            publish = getattr(self.logger, "set_launcher", None)
            if callable(publish):
                publish("RUN" if self.active() else state, self.vertical)

    def fault(self, reason):
        if self.state != "FAULT":
            self.reason = reason
            self._state("FAULT", reason + "; explicit neutral Start reset required")
            self.manip.cancel()  # Suppress held manual inputs; no actuator write.

    def cancel(self, reason, reset_inputs=True):
        if reset_inputs:
            self.shoot_released = self.mode_released = False
        if self.active():
            self.fault(reason)

    def reset_fault(self):
        if self.state in ("FAULT", "INHIBITED"):
            self.reason = ""
            self._state("IDLE", "operator reset; reload before next N1")

    def inputs(self, shoot, mode):
        fresh = bool(shoot and self.shoot_released)
        toggle = bool(mode and self.mode_released)
        self.shoot_released, self.mode_released = not shoot, not mode
        if toggle and not self.active():
            self.vertical = not self.vertical
            self._state(self.state, "release mode " + ("vertical +Z" if self.vertical else "last manual pitch"))
        return fresh and not mode  # Conflicting buttons cannot start a shot.

    def start(self):
        if self.active() or self.state in ("FAULT", "INHIBITED"):
            return False
        self.events = []
        self.elapsed_us = 0
        self.release_commands_us = self.latch_open_est_us = self.ball_detach_est_us = None
        try:
            if not callable(self.clock) or not callable(self.diff):
                raise ValueError("ticks_us / ticks_diff unavailable")
            self.epoch = self.clock()
            release = 0.0 if self.vertical else self.manip.last_manual_pitch
            self.plan = self.model.plan(release)
            if not self.manip.release_allowed():
                raise ValueError("grip reference / release sweep clearance unavailable")
            self.current_inertia = self.model.inertia()
        except (ValueError, OSError) as exc:
            self.reason = str(exc)
            self._state("INHIBITED", self.reason + "; neutral Start reset required")
            return False
        try:
            self._state("HOMING", "commanded start %d deg; physical pose estimated" % self.plan["start_deg"])
            if not self.manip.pitch.command(self.plan["start_deg"]):
                raise ValueError("pitch home command dropped")
            self.home_due_us = self._elapsed() + self.model.calibration["home_duration_us"]
        except (ValueError, OSError) as exc:
            self.fault(str(exc))
            return False
        return True

    def _loaded_angle(self, elapsed):
        c = self.model.calibration
        seconds = max(0.0, (elapsed - self.stroke_us) / 1000000)
        return self.plan["start_deg"] + motion_distance(seconds,
            c["loaded_accel_rad_s2"], c["loaded_speed_rad_s"]) * 180 / math.pi

    def _release(self):
        c, p = self.model.calibration, self.plan
        if not self.manip.release_allowed():
            raise ValueError("release sweep blocked")
        if abs(self._loaded_angle(self._elapsed() + p["latency_us"]) - p["release_deg"]) > c["release_tolerance_deg"]:
            raise ValueError("missed release timing tolerance; grip kept closed")
        stamps = []
        for servo, angle in zip((self.manip.grip_left, self.manip.grip_right), LAUNCH_RELEASE_DEG):
            stamps.append(self._elapsed())
            if not servo.command(angle):
                raise ValueError("grip release command dropped / partial release")
        self.release_commands_us = tuple(stamps)
        self.latch_open_est_us = tuple(stamp + c["grip_latency_us"] for stamp in stamps)
        self.ball_detach_est_us = max(self.latch_open_est_us) + c["ball_detach_delay_us"]
        # Check native call cost as well as scheduler lateness; no physical acknowledgement.
        if abs(self._loaded_angle(self._elapsed() + p["latency_us"]) - p["release_deg"]) > c["release_tolerance_deg"]:
            raise ValueError("release dispatch exceeded timing tolerance; physical release unverified")
        angle = self._loaded_angle(self.ball_detach_est_us)
        seconds = (self.ball_detach_est_us - self.stroke_us) / 1000000
        omega = min(c["loaded_speed_rad_s"], seconds * c["loaded_accel_rad_s2"])
        if angle >= p["brake_deg"] or omega > c["unloaded_speed_rad_s"]:
            raise ValueError("insufficient post-release stopping travel")
        self.brake_us = self.ball_detach_est_us + motion_time((p["brake_deg"] - angle) * math.pi / 180,
            c["unloaded_accel_rad_s2"], c["unloaded_speed_rad_s"], omega) * 1000000
        self.complete_us = self.brake_us + c["braking_duration_us"]
        if self.logger is not None:
            self.logger.emit("INFO", "launch", "release commands t=%d/%d us; latch open EST=%d/%d; ball detach EST=%d" %
                (stamps[0], stamps[1], self.latch_open_est_us[0], self.latch_open_est_us[1], self.ball_detach_est_us))

    def service(self):
        if not self.active():
            return
        try:
            now = self._elapsed()
            c = self.model.calibration
            if self.state == "HOMING" and now >= self.home_due_us:
                self._state("LOAD", "operator-loaded grip assumed; no automatic closing")
                self._state("ARMED", "calibrated model and release clearance accepted")
                self.stroke_us = self._elapsed()
                if not self.manip.pitch.command(self.plan["target_deg"]):
                    raise ValueError("pitch overtravel command dropped")
                self.stroke_us += c["response_delay_us"]
                self.release_due_us = self.stroke_us + self.plan["trigger_us"]
                self._state("ACCELERATING", "one overtravel target; no pitch stop at release")
                now = self._elapsed()
            if self.state == "ACCELERATING" and now >= self.stroke_us:
                self._state("RELEASE_PENDING")
            if self.state == "RELEASE_PENDING":
                if self.release_commands_us is None and now >= self.release_due_us:
                    self._release()
                    now = self._elapsed()
                if self.ball_detach_est_us is not None and now >= self.ball_detach_est_us:
                    self.current_inertia = self.model.inertia(False)
                    self._state("RELEASED", "ball detached ESTIMATED; arm-only inertia")
                    self._state("OVERRUN")
            if self.state == "OVERRUN" and now >= self.brake_us:
                self._state("DECELERATING", "estimated internal-servo braking; no new target")
            if self.state == "DECELERATING" and now >= self.complete_us:
                self._state("COMPLETE", "estimated rest; no physical confirmation / auto reload")
                self.manip.cancel()
        except (ValueError, OSError) as exc:
            self.fault(str(exc))


class Dispatcher:
    def __init__(self, hardware, ticks_diff, clock=None, logger=None):
        self.hw = hardware
        self.diff = ticks_diff
        self.clock = clock
        self.logger = logger
#         self.cranker = Cranker()
        self.armed = False
        self.arm_released = False
        self.request = None
        self.left_target = 0.0
        self.right_target = 0.0
        self.manipulators = Manipulators(hardware, ticks_diff, logger)
        self.launcher = Launcher(self.manipulators, logger)
#         self.crank_last = None
        self.outputs_stopped = False
        self.reason = "boot"

    def halt(self, reason, disarm=True):
        changed = reason != self.reason or (disarm and self.armed)
        if disarm:
            self.armed = False
            self.arm_released = False
        self.reason = reason
        self.left_target = self.right_target = 0.0
#         self.cranker.cancel()
        self.manipulators.cancel()
        self.launcher.cancel(reason, disarm)
        if not self.outputs_stopped:
            self.hw.stop()
            self.outputs_stopped = True
#             self.crank_last = 0
        # Report after zero-output writes, never delay the stop with screen I/O.
        if changed and self.logger is not None:
            level = "INFO" if reason in ("waiting for input", "waiting for neutral + Start") else "WARN"
            self.logger.emit(level, "drive", reason)

    def step(self, request, stop_latched, now, dt):
        if request is not None:
            self.request = request
        r = self.request
        if stop_latched:
            # Keep an actual stop as the latest state. If motion overwrote the
            # stop, discard it and require a NEW release + press to re-arm.
            stopped = r is not None and (not r.connected or r.estop)
            self.request = r if stopped else None
            self.halt("disconnect / emergency stop" if stopped else "stop latched")
            return
        if r is None:
            # No input yet, or an overwritten stop awaiting a fresh sample.
            # Both are already stopped; don't invent a new timeout every tick.
            self.halt("waiting for input" if self.reason == "boot" else self.reason)
            return
        if not r.connected or r.estop:
            self.halt("disconnect / emergency stop")
            return
        if self.diff(now, r.stamp) > REQUEST_TIMEOUT_MS or self.diff(now, r.stamp) < 0:
            self.halt("input timeout")
            return
        #if dt <= 0 or dt * 1000 > CONTROL_GAP_MS:
            #self.halt("control overrun")
            #return
        self.manipulators.select_step(r.step_previous, r.step_next)
        shoot = self.launcher.inputs(r.shoot, r.release_mode)
        # A fresh release + press after every fault prevents reconnect auto-resume.
        arm_pressed = r.arm and self.arm_released
        self.arm_released = not r.arm
        if not self.armed:
            # Legacy crank/power requests are ignored while catapult is disabled.
            # neutral = r.left == 0 and r.right == 0 and not r.crank and not r.power_adjust
            neutral = r.left == 0 and r.right == 0
            if arm_pressed and neutral and not r.grip_left and not r.grip_right and not r.pitch and not r.shoot:
                self.armed = True
                self.arm_released = False
                self.reason = "armed"
                if self.logger is not None:
                    self.logger.emit("INFO", "drive", "armed; sticks control drive")
            else:
                self.halt("waiting for neutral + Start", disarm=False)
                return
        manual = bool(r.grip_left or r.grip_right or r.pitch)
        if arm_pressed and not manual and not r.shoot and r.left == 0 and r.right == 0:
            self.launcher.reset_fault()
        if self.launcher.active() and manual:
            self.launcher.fault("manual servo input interrupted shot")
            manual_interrupted = True
        else:
            manual_interrupted = False
        if shoot and not manual:
            self.launcher.start()
        self.service_launch(now)
        # Send bounded RPM/power targets; optional host ramp stays parked.
        limit = drive_limit()
        self.left_target = clamp(r.left, -limit, limit)
        self.right_target = clamp(r.right, -limit, limit)
        self.outputs_stopped = False  # A partial actuator write must still be stopped.
        self.hw.drive(self.left_target, self.right_target)
        if self.reason != "running":
            self.reason = "running"
            if self.logger is not None:
                self.logger.emit("INFO", "drive", "raw power; PID bypassed" if PID_BYPASS else
                                 "onboard PID speed control active")
        if not self.launcher.active() and not manual_interrupted:
            self.manipulators.update(r.grip_left, r.grip_right, r.pitch, now, dt)
#         previous_power = self.cranker.power
#         power = self.cranker.update(r.crank, r.power_adjust)
#         if self.logger is not None and self.cranker.power != previous_power:
#             self.logger.emit("INFO", "crank", "selected power %d%%" % self.cranker.power)
#         if power != self.crank_last:
#             self.hw.crank(power)
#             self.crank_last = power
#             if self.logger is not None:
#                 self.logger.emit("INFO", "crank", "output %d%%" % power)

    def service_launch(self, now):
        if self.launcher.active():
            age = self.diff(now, self.request.stamp)
            if not self.armed or age < 0 or age > REQUEST_TIMEOUT_MS:
                self.halt("input timeout")
                return
            self.launcher.service()


def dispatch_loop(hardware, state, logger=None):
    """One input/actuator owner, following the upstream single-loop sample.

    Field-tool runtime is commented out. Console I/O stays outside this cycle.
    """
    controller = Dispatcher(hardware, time.ticks_diff, time.ticks_ms, logger)
    previous_link = None
    previous_input_error = None
    last = None
    stage = "initial stop"
    try:
        hardware.stop()
        controller.outputs_stopped = True
        if logger is not None:
            logger.emit("INFO", "sched", "input/control worker online")
        while not state.shutdown:
            stage = "sample gamepad"
            started = time.ticks_ms()
            try:
                # Field-tool input path is temporarily disabled.
                # request = service.sample() if service is not None else None
                # remote_input = request is not None
                # if not remote_input:
                #     request = hardware.sample()
                # elif hardware.local_stop():
                #     request = DriveRequest(started, estop=True)
                request = hardware.sample()
                if logger is not None:
                    logger.set_gamepad_waiting(not request.connected)
                input_error = None
            except Exception as exc:
                request = DriveRequest(started, estop=True)
                input_error = str(exc)
                if logger is not None:
                    logger.set_gamepad_waiting(True)
            # Sample/API time counts toward both freshness and the cycle deadline.
            now = time.ticks_ms()
            dt = (time.ticks_diff(now, started) + CONTROL_PERIOD_MS if last is None
                  else time.ticks_diff(now, last)) / 1000.0
            last = now
            if state.shutdown:
                break
            stage = "dispatch outputs"
            controller.step(request, request.estop or not request.connected, now, dt)
            stage = "publish diagnostics"
            if (controller.reason in ("input timeout", "control overrun") and
                    controller.reason != state.reason):
                state.timing_faults += 1
            state.armed = controller.armed
            state.reason = controller.reason
            state.cycles += 1
            state.max_cycle_ms = max(state.max_cycle_ms, int(round(dt * 1000)))
            state.launch_state = controller.launcher.state
            state.launch_reason = controller.launcher.reason
            # if service is not None:
            #     service.fieldtool.armed = controller.armed
            # Publish diagnostics only after applying the safety/output decision.
            if logger is not None:
                if input_error is not None:
                    if input_error != previous_input_error:
                        logger.emit("WARN", "input", "waiting; outputs disabled: " + input_error)
                elif request.connected != previous_link:
                    previous_link = request.connected
                    logger.emit("INFO" if request.connected else "WARN", "gamepad",
                                "connected; neutral + Start required" if request.connected else
                                hardware.gamepad_wait_reason + "; waiting; local outputs disabled")
            previous_input_error = input_error
            spent = time.ticks_diff(time.ticks_ms(), started)
            # Keep gamepad/wheel traffic at its existing cadence. Only the launch
            # scheduler runs between those samples, with the same actuator owner.
            while controller.launcher.active() and not state.shutdown and spent < CONTROL_PERIOD_MS:
                stage = "service launch"
                controller.service_launch(time.ticks_ms())
                state.launch_state = controller.launcher.state
                state.launch_reason = controller.launcher.reason
                time.sleep_ms(LAUNCH_SERVICE_MS)
                spent = time.ticks_diff(time.ticks_ms(), started)
            time.sleep_ms(max(1, CONTROL_PERIOD_MS - spent))
    except Exception as exc:
        state.failure = exc
        state.failure_stage = stage
    finally:
        state.close()
        controller.launcher.cancel("control worker shutdown")
        state.launch_state = controller.launcher.state
        state.launch_reason = controller.launcher.reason
        try:
            hardware.stop()
        except Exception as exc:
            if logger is not None:
                logger.emit("ERROR", "motor", "stop unconfirmed: " + str(exc))
        if logger is not None:
            if state.failure is not None:
                logger.emit("ERROR", "dispatch", state.failure_stage + ": " + str(state.failure))
            logger.emit("WARN", "sched", "control worker stopped")
        state.armed = False
        state.done()


def main():
    global app_state
    boot_mark("03 importing cyberpi")
    import cyberpi
    import mbot2
    boot_mark("04 cyberpi imported; creating logger")
    logger = KernelLog(time.ticks_ms, time.ticks_diff)
    logger.emit("INFO", "boot", "mBot2 / CyberPi uPython")
    # A safe zero-output attempt precedes capability validation and thread startup.
    boot_error = None
    drive_api = "drive_power" if PID_BYPASS else "drive_speed"
    for name, args in ((drive_api, (0, 0)),
                       ("motor_set", (0, CRANKER_PORT))):
        boot_mark("05 zero output %s %s" % (name, str(args)))
        try:
            getattr(mbot2, name)(*args)
        except Exception as exc:
            logger.emit("ERROR", "motor", "boot zero " + name + ": " + str(exc))
            if boot_error is None:
                boot_error = exc
    # Initialize the screen only after both wheel/M1 zero-output attempts.
    boot_mark("06 zero attempts complete; attaching screen")
    logger.attach_screen(cyberpi)
    logger.emit("INFO", "boot", "06 screen init attempted; outputs zeroed if available")
    logger.pump(force=True)
    boot_mark("07 screen init returned; LCD %s" % ("available" if logger.screen is not None else "disabled/unavailable"))
    if boot_error is not None:
        logger.emit("ERROR", "init", "motor initialization failed")
        logger.pump(force=True)
        raise boot_error
    logger.emit("INFO", "motor", "EM1 EM2 M1 outputs zero")
    boot_mark("08 importing gamepad")
    logger.emit("INFO", "boot", "08 importing gamepad")
    logger.pump(force=True)
    try:
        import gamepad
    except ImportError as exc:
        gamepad = None
        logger.emit("WARN", "gamepad", "module unavailable; waiting: " + str(exc))
    boot_mark("09 gamepad checked; validating hardware")
    try:
        hardware = CyberPiHardware(cyberpi, gamepad, mbot2)
    except Exception as exc:
        logger.emit("ERROR", "init", str(exc))
        logger.pump(force=True)
        raise
    logger.set_gamepad_waiting(True)
    logger.emit("WARN", "input", hardware.gamepad_wait_reason + "; local outputs disabled while waiting")
    logger.emit("INFO", "drive", "%s; %s; max %d %s" % (
                DRIVE_MODE, "PID bypass" if PID_BYPASS else "onboard PID", drive_limit(),
                "% power" if PID_BYPASS else "RPM"))
    logger.emit("INFO", "manip", "%s/%s independent beams, neutral %d/%d; %s pitch -45..135" %
                (GRIP1_PORT, GRIP2_PORT, GRIP1_NEUTRAL_DEG, GRIP2_NEUTRAL_DEG, PITCH_PORT))
    logger.emit("INFO", "controls", "Lx/Rx beams; L1/R1 pitch; L2/R2 step 1,2,5,15,30,60 deg; clearance 10 mm")
    logger.emit("INFO", "launch", "N1 shoot; N2 vertical/manual release; neutral Start resets shot fault; calibration required")
    logger.emit("INFO", "pitch", "MECDS-150 manufacturer: 6 V; peak stall 1.61809725 Nm; no-load 5.5116 rad/s; not operating guarantees")
    state = WorkerState()
    app_state = state
    # service = prepare_field_tool(logger)  # Field tool temporarily commented out
    boot_mark("10 hardware ready; starting control worker; stack request %d bytes" % CONTROL_STACK_BYTES)
    logger.emit("INFO", "boot", "10 starting control worker")
    logger.pump(force=True)
    previous_stack = None
    try:
        # MicroPython uses this error for stack exhaustion as well as recursion.
        # Set before creation; querying stack_size() can reset it to the default.
        previous_stack = _thread.stack_size(CONTROL_STACK_BYTES)
        _thread.start_new_thread(dispatch_loop, (hardware, state, logger))
    except Exception as exc:
        try:
            hardware.stop()
        finally:
            logger.emit("ERROR", "sched", "worker start failed: " + str(exc))
            logger.pump(force=True)
        raise
    finally:
        if previous_stack is not None:
            try:
                # Do not change stack allocation for later vendor-created threads.
                _thread.stack_size(previous_stack)
            except Exception as exc:
                logger.emit("WARN", "sched", "stack default restore failed: " + str(exc))
    boot_mark("11 control worker launched; field tool commented out")
    # if service is not None:
    #     service.start()
    boot_mark("12 input loop ready; neutral + Start; sticks drive")
    logger.emit("INFO", "init", "12 ready: neutral + Start; sticks drive")
    try:
        while not state.is_done():
            logger.pump()
            time.sleep_ms(LOG_FLUSH_MS)
    finally:
        state.close()
        # if service is not None:
        #     service.close()
        # Worker owns all output writes until it exits. Bounded wait, no kill API.
        started = time.ticks_ms()
        while not state.is_done() and time.ticks_diff(time.ticks_ms(), started) < 1000:
            time.sleep_ms(10)
        if not state.is_done():
            logger.emit("ERROR", "sched", "worker blocked; switch off shield power")
        else:
            logger.emit("INFO", "halt", "application stopped")
        logger.pump(force=True)
        # Full native traceback from main only after the worker's stop attempts.
        # Retain the exception in app_state for diagnosis; never restart on fault.
        if state.is_done() and state.failure is not None and LOG_SERIAL_ENABLED:
            try:
                sys.print_exception(state.failure)
            except Exception as exc:
                boot_mark("traceback unavailable: " + str(exc))


def is_application_entry(name, implementation):
    # CyberOS imports an uploaded project into a mainN program slot.
    return name == "__main__" or (implementation == "micropython" and
            name.startswith("main") and name[4:].isdigit())


if is_application_entry(__name__, sys.implementation.name):
    try:
        main()
    except Exception as exc:
        boot_mark("FAILED: " + str(exc))
        raise

# CyberPi gamepad API cross-check

ADR imports **`gamepad`** directly and uses **`gamepad.is_online()`** for link
status. The previous `is_connected` default was incorrect for the retained
CyberOS firmware and blocked local input even when the controller was paired.

## Examples and loader dependency

The supplied [PerfecXX/mBot2 button example](https://github.com/PerfecXX/mBot2/blob/main/example/micropython/gamepad/01-Get_Pressed.py)
uses `import cyberpi, gamepad` and `gamepad.is_key_pressed(name)`. Its
[joystick example](https://github.com/PerfecXX/mBot2/blob/main/example/micropython/gamepad/02-Get%20Analog.py)
uses `gamepad.get_joystick(name)` with `Lx`, `Ly`, `Rx`, `Ry`. Those examples do
not establish a connection-status method.

ADR reads `Lx` / `Rx` with `get_joystick` to control the left/right direct-drive
beams independently. Ly/Ry continue to drive the wheels in default tank mode;
L1/R1 are boolean pitch raise/lower inputs. Beam-axis values use the same signed
±100 input convention as the retained joystick sample. Beyond the dead zone,
only the sign selects a single atomic angle increment; center the axis before
another move. L2/R2 select the previous/next increment from 1°, 2°, 5°, 15°,
30°, 60° once per press, and the ready screen shows that selection. Stick clicks
are unused. N1 starts one pitch/grip shot; N2 toggles release at vertical
(+Z, 0°) versus the last manually commanded pitch angle. Both grip release
targets are 90°. Holding N1 cannot repeat/queue a shot. The shot profile starts
inhibited until mechanical travel and motion/release calibration are supplied.
Servo commands use `cpi.mbot2.servo_add` without servo feedback or automatic
repeat; wheel encoder PID is unchanged. See the
[manipulator model](fw_adr_architecture.md#manipulators) for clamping and clearance.

`LoaderStub.mblock` declares the `firefly_bluetoothcontroller` extension.
The [official extension source](https://mblock-extend.oss-cn-shenzhen.aliyuncs.com/extlist/prod/extract/1810855433942863872/3d00cd92-3358-468f-8dd4-48d0b25f2f3b/firefly_bluetoothcontroller/index.js)
(version 0.0.3) generates those same top-level `gamepad` imports and calls.
It declares no upload middleware or extra Python library payload. ADR's
`import gamepad` therefore matches the selected extension; selecting the
extension in the stub does not itself execute a Python import in ADR.

## Connection API from the actual firmware artifact

Artifact: `fw/static_analysis_adr/fw_dump_cyberpi_head/flash.bin`.
SHA-256: `aeff183eb0397b15fb13b58d09191377de09736afcfc6d4437dbfdc0f6bf4083`.
Offsets below are absolute offsets in that dump.

The frozen `gamepad.py` module exports `is_online` under QSTR ID 2479 (`0x09af`).
The module's table includes `MAKE_FUNCTION 35; STORE_NAME is_online` around
`0x1c7662`. Its source-file QSTR is 6053 (`0x17a5`). The relevant QSTR entries
are `gamepad_o` = 6023 (`0x1787`), `gamepad_online_flag` = 6048 (`0x17a0`),
`__online_detect_task` = 6050 (`0x17a2`), `time` = 3481 (`0x0d99`), and
`sleep` = 3296 (`0x0ce0`).

The public function body at `0x1c775c..0x1c775f` is:

```text
1c a0 17    LOAD_GLOBAL gamepad_online_flag
5b          RETURN_VALUE
```

The background task at `0x1c7723` calls `gamepad_o.is_online()`, stores the
result in `gamepad_online_flag`, then calls `time.sleep(1)` and loops. Equivalent
behavior is:

```python
def __online_detect_task():
    global gamepad_online_flag
    while True:
        gamepad_online_flag = gamepad_o.is_online()
        time.sleep(1)

def is_online():
    return gamepad_online_flag
```

This is a reconstruction of those instructions, not recovered Python source.
Opcode interpretation follows upstream [MicroPython v1.10 bc0.h](https://github.com/micropython/micropython/blob/v1.10/py/bc0.h).
The public status is cached, initially false, and can lag native link changes
by the background polling interval plus scheduling delay. ADR's 200 ms request
freshness check concerns software requests, not a promise of 200 ms physical
controller-disconnect detection. Reading the axis value zero is not used as a
connectivity substitute.

## Physical read-only retest

With the controller connected, the device emitted the same GATTS connection
message reported by the user, followed by `Gamepad attitude system initialized`.
A read-only `gamepad.is_online()` call during initialization returned `False`.
After startup, three samples several seconds apart returned:

```text
GP_LINK True True
GP_LINK True True
GP_LINK True True
```

Each line reports public `gamepad.is_online()` and native
`gamepad.gamepad_o.is_online()`, respectively. This verifies the method is
imported and reports the connected controller; it does not exercise controls.
The initial false result is consistent with the cache/background initialization
observed in the dump. No raw BLE parser takeover was active: the installed ADR
configuration had `FIELD_TOOL_ENABLED = False`.

The source and probe now select `is_online`. Missing/false/invalid status keeps
the safe waiting behavior; neutral/Start arming and deadman gates remain.

The corrected 42,325-byte ADR source was uploaded as `/flash/_xx_ADR.py`.
Source SHA-256:
`d3d6419fc119a9654757c5575178ae1f494d9fb7eda2593af2597e9748c56290`.
A subsequent boot reached marker 12 and emitted:

```text
[ADL  2.391] INFO gamepad: connected; neutral + Start required
```

The robot remained unarmed. Repeated `input timeout` and occasional
`control overrun` records were still observed; timing and physical movement
are not qualified by this API correction. Host regressions passed: 24 ADR,
15 CLI/protocol, and five upload-script checks (44 total).

Subsequent work established an unchanged upstream joystick/display baseline and
a user-confirmed operational EM1 `drive_speed` sample, then integrated that
native speed-control path into ADR. See the current
[architecture](fw_adr_architecture.md) and
integration validation (internal record); the captures
below describe the earlier API-correction revision.

Retained captures:

  and exact RX hex (internal record).
  and exact RX hex (internal record).

The raw RX SHA-256 hashes are respectively
`c862283cb926a7ae1e817b8a9392499f5d5ff9fa6b84df5598288ab87abce80c`
and `92bdadc599a757817daa9994efa926aa140881ecc77ce024449bee6ad9e33611`.

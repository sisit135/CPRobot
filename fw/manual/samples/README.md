# Upstream gamepad baseline

`gamepad_analog_upstream.py` is an unchanged copy of
[PerfecXX/mBot2: 02-Get Analog.py](https://github.com/PerfecXX/mBot2/blob/18111ba5ac07c8ad9f056dd9cc2a80d8ecbf6192/example/micropython/gamepad/02-Get%20Analog.py),
commit `18111ba5ac07c8ad9f056dd9cc2a80d8ecbf6192`.
SHA-256: `cd51cd6f067d2b92fd61fb97130189db8092b4a93bc596ee863f914f7cf930ea`.
It reads four joystick axes and displays them with the sample's original
`cyberpi.display.show_label` calls. It contains no motor commands, dispatcher,
PID loop, application threads or logging framework.

The same repository's
[basic movement example](https://github.com/PerfecXX/mBot2/blob/18111ba5ac07c8ad9f056dd9cc2a80d8ecbf6192/project/01-mBot2_bluetooth_control/01-basic_movement.py)
uses `mbot2.drive_speed(v_left, v_right)`, with `Ly + Rx` for the left wheel
and `Rx - Ly` for the right. This uses the onboard speed controller. That
example has no arm/deadman gate; the baseline check here does not run its
motion loop.

`gamepad_em1.py` adapts that movement call for the requested EM1-only check.
Hold R2 and move Ly to command up to ±20 RPM, with a ±10 stick dead zone.
Release R2, press Select, or press the CyberPi B button to command zero.
`drive_speed(rpm, 0)` keeps EM2 at zero; M1 is also zeroed at entry and exit.
The screen shows the requested RPM, and USB prints `EM1_RPM` when it changes.
This is a single loop without ADR's dispatcher or host speed PID. The public
`gamepad.is_online()` status uses the firmware's cached link flag; see
[API evidence](../../../docs/adr_gamepad_api.md) for its disconnect latency.

Upload/monitor commands and prerequisites are in the
[build guide](../../../docs/building.md#adr-usb-serial-cli).

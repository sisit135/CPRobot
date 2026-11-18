# THS color-disabled, pose-only completion validation, 2026-11-19

## Change

The user requested a re-check of both buses and all ToFs with TCS physically
removed, followed by disabling TCS acquisition and optical end confirmation.
This follows the [connected/disconnected investigation](ths_i2c_isolation_2026-11-19.md).
The local PB13 motor nSLEEP override is present in the tested images and is
excluded from this change.

`AXI_TCS3472_ENABLE` now defaults to zero. Normal sensor initialization skips
TCS; the sensor task and boot snapshot compile out color acquisition. Retained
compatibility hooks return false without bus access. Legacy driver fixtures
explicitly enable the polling implementation. The generic address inventory
still probes reserved `0x29`, labeled Color disabled. Its address probe is not
a color-driver transaction or identity check.

Navigation no longer consumes tape events or waits for a tape latch. After
executor DONE, completion requires a finite, tracking independent FEATURE pose
within 12 mm of the commanded cell center, timestamped at/after that goal
request and no more than 150 ms old, with no future timestamp. Executor
stopped-output/gyro/pose settling and the absolute 86-second deadline remain.
An independent pose at the goal center during RHR causes a stop and executor
replan for settling rather than issuing GOAL directly. Optical compatibility
telemetry fields remain false. EKF equations, compass qualification, turn
clearance and trace diagnostic service/motion blocking are unchanged.

## Live transport and ranging checks

The prior disconnected-TCS matrix completed 17 software-reset trials with
6/6 IMU and 6/6 all-three-ToF initialization attempts passing. The new bench
case 13 then polled raw acceleration, magnetic field and sequential ToF
measurements for 30 seconds, without TCS or motor calls. Three initial trials
passed with 2,645 completed ToF measurements in total and zero API errors or
timeouts. Left measurements were rejected/out of range in that scene; these
proved communication progress, not accepted optical ranges.

After the user placed a target at the left sensor, one subsequent attempt
failed: left `id=0`, boot stage 1, error `-7`, before the ranging loop. I2C2
was idle/READY with physical lines high. This is retained as an intermittent
startup failure; the previous successful trials do not erase it. A separate
debugger retry timed out before reaching main and is excluded from sensor
results.

Bench-only instrumentation now records all-low XSHUT pin readback and address
responses at `0x29/0x30/0x31/0x32`, plus released-pin readback. A failed boot
probe also reports a bounded diagnostic read of the model register. These
checks are excluded from trace/production builds. The normal 2 ms release
wait and 100 ms bounded boot-probe window are retained. ST specifies a maximum
1.2 ms sensor boot time in the [VL53L0X datasheet](https://www.st.com/resource/datasheet/vl53l0x.pdf);
the tests do not measure the module's electrical waveform or rail behavior.

The instrumented left-target trial passed:

| Sensor | Accepted ranges | Rejected ranges | API errors | Timeouts |
| --- | ---: | ---: | ---: | ---: |
| Left | 264 | 0 | 0 | 0 |
| Front | 263 | 1 | 0 | 0 |
| Right | 264 | 0 | 0 | 0 |

All XSHUT readbacks were low with no address responses while reset was
asserted; each released pin subsequently read high. Both IMU streams progressed.
The single front rejection was a zero range, kept separate from bus failures.

The requested final ToF re-check then completed ten ToF-only reset/startup
cycles followed by two more 30-second all-sensor trials. All twelve device
initialization attempts passed. Both soak trials passed with accepted ranges
from all three ToFs and zero API errors/timeouts. Settings flash remained
byte-identical throughout. These results validate the observed bench sessions;
they do not establish a repeated-POR failure rate or resolve the earlier
intermittent left boot timeout.

The final user-confirmed full-system power cycle passed with TCS disconnected.
The reset CSR was `0x0e000000`. Before any device HAL, the GPIO inventories
found ACCEL `0x19` and MAG `0x1e` on the main bus and the shared ToF boot address
`0x29` on I2C2. After initialization, all three assigned ToF addresses
`0x30/0x31/0x32` responded and both buses were idle/READY with zero HAL errors.
The boot snapshot obtained accepted left/front/right ranges of 32/10/95 mm.
A subsequent read-only RAM capture confirmed all three online, each with one
accepted sample, no rejected samples, zero API errors and zero timeouts;
ACCEL/MAG were online with zero bus errors. Flash readback matched the final
trace image and settings remained unchanged. This is one observed cold-start
pass, not repeated-POR qualification.

## Firmware and native checks

Isolation, trace and production profiles compile/link. RAM use is 27,644 bytes.
Application flash use is 234,300 bytes for trace, 225,676 for production, and
within the 262,144-byte application allocation for isolation. Settings sectors
are preserved. The color-disabled trace image was programmed/verified and
completed three startup trials: the expected terminal condition is
`sensor diagnostics complete; services/localization halted`, not a TCS
identity failure. A matching read-only flash/RAM capture showed both controllers
READY (`0x20`), both error codes zero, IMU/magnetometer online and all ToFs
online without API errors/timeouts. The trace image is restored after bench
testing. All test images kept application services and motors stopped.

Final binary SHA-256 values:

| Profile | SHA-256 |
| --- | --- |
| Trace (left on target) | `567d067f1f1639a3cadddf9ea25db10802e431c374e76e956306953cde1c7945` |
| Production (compile only) | `5234ff02726f6f9f0e03c6a939083aeb7873b26beb6266f930c7a087475b7428` |
| Isolation | `9b36e4d744f627eb7a7fdae38331f54c25e8acf5b69cca5480af8013dedbbff1` |

The full native run has 121 passes and 31 failures out of 152 tests. All changed
HAL, BUP, boot-snapshot, isolation, feature-runtime, end-cell and no-tape
open-arena completion checks pass. Fault-injection cases verify that the soak
rejects an IMU bus error, a ToF API error and lack of measurement progress.
The open-arena lifecycle completes with no optical event and verifies physical
goal/stop behavior. The final focused rerun passed all 64 selected tests,
including the LSM HAL, BUP, isolation faults, schema and no-tape completion.

The remaining failures are disclosed rather than weakening checks:

- Twenty-five LSM lifecycle failures reproduce on `ab1bbd2` in an isolated
  source snapshot. The baseline needs only the missing simulator bus-init stub
  to link against the already-introduced BUP bus interface.
- Four turn-guard/startup-turn assertion failures also reproduce on that
  baseline without this change.
- The wiring-document test rejects the user's local PB13 override against the
  retained PB12 drawing.
- The supplied-maze no-tape case now expects positive pose-only completion and
  fails. Running that exact scene on the old code without the expected-failure
  wrapper gives the same TIMEOUT at `(0.8984, 0.1083)`, blocked waiting for turn
  clearance before reaching the goal. The independent open-arena no-tape test
  passes. Full navigation and the gyro-free backend remain unqualified.

## Local evidence

- `/tmp/ths-no-tcs-soak-summary.log`: three initial 30-second sessions.
- `/tmp/ths-left-target-soak-summary.log`: retained intermittent boot failure.
- `/tmp/ths-tof-reset-diag-summary.log`: target-present accepted-range check.
- `/tmp/ths-tof-repeat-confirm-summary.log` and `...-results.json`: final ten
  startup cycles and two soak trials, with separate completion/pass fields.
- `/tmp/ths-pose-only-final-tests.log`: full native results;
  `/tmp/ths-pose-baseline-tests.log`,
  `/tmp/ths-pose-baseline-lifecycle-tests.log`, and
  `/tmp/ths-pose-baseline-no-tape.log`: baseline comparisons.
- `/tmp/ths-pose-only-trace-summary.log`,
  `/tmp/ths-pose-only-final-state.json`, and final trace capture: deployed
  diagnostic behavior and controller/sensor state.
- `/tmp/ths-pose-only-cold.log` and `/tmp/ths-pose-only-cold-state.json`:
  final full power-cycle capture and read-only flash/RAM/controller verification.
- `/tmp/ths-pose-only-commit-tests.log`: final 64/64 focused rerun.

Settings sectors `0x08040000..0x0807ffff` retain SHA-256
`6b02ca41debc2f6be2ae2f7875e2da089ab08c49df503ae27dc3b278cb19ca77`.

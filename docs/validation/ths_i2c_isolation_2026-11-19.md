# THS I2C initialization isolation, 2026-11-19

## Scope

This follows the [early POR investigation](ths_i2c_por_2026-11-19.md).
The dedicated isolation profile runs one selected initialization case, replacing
normal bring-up and blocking all application services. Default case 1 prepares
only I2C1 and initializes TCS. Each software reset restores C/C++ state and
case selection; attached sensor modules retain power. These experiments are
not equivalent to independent cold-start trials.

See [building](../building.md#i2c-initialization-isolation) for profile commands,
the full case table and RAM selection. The deployed builds include the user's
existing local PB13 motor nSLEEP override, which is excluded from this change.

## Before the polling fix

The initial isolation image was programmed/verified through ST-Link. Seventeen
software-reset experiments completed: all thirteen supported cases, with
additional TCS-only, IMU-only and ToF-only repetitions. No motion services
started. Settings flash matched before/after the matrix.

| Isolated initialization | Observed result |
| --- | --- |
| I2C1 controller only, followed by address probes | LSM addresses ACK; main `0x29` no ACK |
| I2C1 and TCS only, without any earlier inventory | TCS register-pointer transfer fails, Wire error 3 |
| I2C1 `Wire.begin()` without the subsequent `setClock()`, then TCS | Same transfer failure |
| I2C1 and IMU only | IMU PASS, repeated twice |
| I2C2 and ToF devices only; I2C1 never initialized | All three ToFs initialize, repeated twice |
| IMU before TCS | IMU PASS; one TCS ID read returns `0x4d`, rejected by the old `0x44`-only gate |
| ToF controller before/after I2C1, then TCS | TCS transfer fails in both orders |
| ToF devices, stopped motor outputs, or settings before TCS | TCS transfer fails in each case |
| Normal device order, with/without initial inventories | IMU/ToF PASS; TCS transfer fails |

TCS-only failed three times. The failure does not require the GPIO inventories,
other controller, IMU, ToF device initialization, motor configuration, settings,
or explicit clock reconfiguration. This isolates the transaction path rather
than establishing electrical integrity of the module.

## Confirmed controller-state failure

Before TCS initialization, I2C1 reported:

```text
SDA=1 SCL=1
CR1=0001 CR2=0030 SR1=0000 SR2=0000
state=20 error=00000000 CCR=00f0 TRISE=31
```

After the failing Wire register-pointer transaction:

```text
register pointer reg=0x12 Wire error=3
SDA=1 SCL=1
CR1=0401 CR2=0330 SR1=0000 SR2=0000
state=28 error=00000004 CCR=00f0 TRISE=31
Scan result: bus busy
```

`0x28` is `HAL_I2C_STATE_LISTEN`, and HAL error `0x04` is AF/NACK. Physical
BUSY (`SR2` bit 1) is clear. The controller has entered slave-listening state,
so subsequent polling calls cannot use it as a READY master. This explains
how a TCS transaction failure can also remove IMU readings on that controller.

The installed STM32 Arduino 3.0.0 core's
`libraries/Wire/src/utility/twi.c`, `HAL_I2C_ErrorCallback()`, calls
`HAL_I2C_EnableListen_IT()` when `obj->isMaster` is true. Its Wire wrapper maps
AF to Wire error 3. Thus error 3 does not by itself establish which byte was
NACKed. This is a different condition from a physically stuck SCL/SDA line.

## Change

TCS register access now uses bounded polling `HAL_I2C_Mem_Read/Write` on the
same startup-owned I2C1 handle as the IMU. Wire still prepares the controller;
TCS no longer uses its interrupt transaction API or that error callback path.
Polling retains the existing command/auto-increment bits, 5 ms transfer timeout,
integration phases, RGBC byte order, freshness behavior and normal failure gate.
No SDK/vendor function or signature is modified.

The user identified the color device as TCS3472. Both documented family IDs
are now accepted, with the actual ID logged: `0x44` (TCS34725) and `0x4d`
(TCS34727). Other IDs remain rejected. These ID assignments and the shared
register model come from the [manufacturer's DS000390 datasheet](https://look.ams-osram.com/m/7ec5bcc3e40679be/original/TCS3472-DS000390.pdf),
ID register on page 27. Interface-level compatibility of the installed module
is not established by accepting its ID.

Independent bus/device HAL initialization primitives are shared by normal
bring-up and the isolation runner. Normal initialization order and device
failure gates are retained. Isolation deliberately reports results and
continues to TCS after preceding device failures; it always blocks services.

## Validation

All 28 focused native tests pass, including every isolation case, invalid-case
rejection, BUP gates, both accepted color IDs, unexpected ID rejection, bounded
polling transfers, conversion freshness, byte order and bus-fault latching.
Isolation, trace and production firmware compile/link successfully. Native
fixtures do not simulate the SDK interrupt failure; live controller snapshots
above establish that behavior.

Initial local evidence:

- `/tmp/ths-isolation-matrix-summary.log` and `ths-isolation-results.json`:
  all pre-fix experiments.
- `/tmp/ths-isolation-00-case-1.log`: TCS-only transfer failure and LISTEN state.
- `/tmp/ths-isolation-05-case-2.log`: the successful `0x4d` ID read.
- `/tmp/ths-isolation-settings-before.bin` / `ths-isolation-settings-after.bin`:
  settings preservation across the initial matrix.

## Post-fix live comparison

After the VS Code debugger released ST-Link, the isolation and trace images
from `3a22d62` were programmed and verified on the target. They include only
the local motor-pin override described above in addition to that revision.

The same seventeen-case software-reset matrix completed after the polling
change:

| Check | Result |
| --- | --- |
| TCS initialization | 1 PASS, 11 FAIL across 12 attempts |
| TCS-only, case 1 | All three attempts still NACK |
| IMU then TCS, case 2 | IMU PASS; TCS ID `0x4d` accepted; configuration writes PASS |
| I2C1 after every failed TCS attempt | READY (`0x20`), AF error `0x04`, physical lines high, BUSY clear |
| Address inventory after every failed TCS attempt | Completes; LSM `0x19` and `0x1e` ACK |
| IMU initialization | All five attempts PASS |
| ToF initialization | All five attempts time out on left (`id=0`) at boot stage, error `-7` |
| Application services | Blocked in every case |

Representative TCS-only result:

```text
1 0 tcs: transfer reg=0x12 status=1 error=00000004
1 0 tcs: identity transfer failed
1 2 i2c1: isolation result: SDA=1 SCL=1
1 2 i2c1: CR1=0401 CR2=0030 SR1=0000 SR2=0000
1 2 i2c1: state=20 error=00000004 CCR=00f0 TRISE=31
```

The controller no longer enters LISTEN after the NACK. Subsequent main-bus
scans finish rather than returning HAL_BUSY. This validates containment of
the shared-controller failure; it does **not** fix the underlying TCS NACK.
The single successful TCS initialization also does not establish reliable
RGBC acquisition.

Three further normal trace bring-up captures completed after software reset.
Each initialized the IMU, timed out on the left ToF at boot, and halted before
TCS initialization. Acceleration and native magnetic readings were available
in the bring-up snapshot; gyro remained N/A. A powered ToF still ACKed at its
previously assigned `0x30` after the failed reset/boot sequence. The cause of
that retained address has not been established. ToF-only case 12 reproduces
the failure without calling TCS, so it cannot be attributed to a failing TCS
transaction in the same boot.

## Post-fix full power cycle

The user then fully power-cycled the robot and sensor supply with the trace
diagnostic image installed. RTT captured a fresh boot, reset CSR `0x0e000000`,
and GPIO inventories before controller/device initialization. Main-bus
`0x19`, `0x1e` and `0x29` ACKed via GPIO; the hardware-controller inventory
still reported no ACK at main `0x29`.

All three ToFs initialized on this cold start. Their assigned addresses
`0x30`, `0x31` and `0x32` ACKed. Normal bring-up reached TCS, which again
failed its identity transfer with AF/NACK. Later boot-snapshot processing
successfully read the IMU and ToFs. Final debugger reads, taken after the
transactions completed, confirmed:

- I2C1 `HAL_I2C_STATE_READY`, error code zero after subsequent IMU transfers;
  CR2 `0x0030`, SR1/SR2 zero, with no slave-listening interrupt enable bits.
- IMU and magnetometer online, zero bus errors; cached acceleration
  `(-3.354, 7.924, -5.806) m/s²`, acceleration timestamp `2139046 µs` and
  magnetic timestamp `2140290 µs`, both after the failed TCS read at 2136 ms.
- All three ToFs online with zero API errors/timeouts. Front and right each
  produced one accepted sample (66 mm and 18 mm); left produced one rejected
  out-of-range sample (`RangeStatus=4`, 8190 mm).
- TCS phase FAULT and BUP status FAILED. Application services remain blocked.

Vendor trace traffic and SWD reconnection caused omissions in RTT output;
the final HAL/BUP/ToF state above was read directly rather than inferred from
missing log lines. This is one observed cold start, not POR qualification.
The target is left running the trace diagnostic image with bring-up halted.

## Artifacts and limits

Settings sectors `0x08040000..0x0807ffff` matched byte-for-byte before/after
the post-fix matrix, trace repetitions and full power cycle. Their SHA-256 is
`6b02ca41debc2f6be2ae2f7875e2da089ab08c49df503ae27dc3b278cb19ca77`.

| Programmed binary | SHA-256 |
| --- | --- |
| `blackpill_f411ce_i2c_isolation/firmware.bin` | `9431f2dfa9446fb67b5bf7ab066a4309633631cb8e4b5cf5be83c00e5c31365a` |
| `blackpill_f411ce_trace/firmware.bin` | `56974a35019398a0503141d7d5bfe8f76d3b27c1e25ed8b8f36dfba2fda3851f` |

Post-fix local evidence:

- `/tmp/ths-isolation-polling-matrix-summary.log`,
  `/tmp/ths-isolation-polling-results.json` and the per-case logs.
- `/tmp/ths-isolation-polling-00-case-1.log`: NACK leaves I2C1 READY and LSMs
  addressable; `/tmp/ths-isolation-polling-05-case-2.log`: accepted `0x4d` ID
  and successful TCS configuration.
- `/tmp/ths-trace-polling-repeat-summary.log` and three per-run logs:
  normal bring-up after software reset.
- `/tmp/ths-trace-polling-cold.log` and
  `/tmp/ths-trace-polling-cold-openocd.log`: user-performed power cycle.
- `/tmp/ths-trace-polling-cold-state.log` and
  `/tmp/ths-trace-polling-cold-imu.log`: final controller, sensor and BUP state.

The polling fix prevents the observed TCS NACK from disabling the shared
controller and IMU. Reliable TCS communication, effective ToF reset through
XSHUT, repeated POR behavior, and qualified navigation remain unresolved.

## Retry with TCS physically disconnected

The user subsequently removed TCS completely from the bus. The same isolation
and trace binaries above were reused without firmware changes. Seventeen
software-reset isolation trials ran in this order:
`9, 0, 10, 11, 12, 2, 3, 4, 5, 6, 7, 8, 9, 1, 11, 12, 1`.
Starting with case 9 captured both GPIO inventories before any device HAL.

| Check with TCS removed | Result |
| --- | --- |
| Main GPIO inventory before Wire/device HALs | LSM `0x19`/`0x1e` ACK; TCS `0x29` no ACK |
| Initial ToF GPIO inventory | Previously assigned `0x30`/`0x31`/`0x32` all ACK |
| IMU initialization | 6/6 PASS |
| All-three-ToF initialization | 6/6 PASS, including both ToF-only trials |
| TCS attempts | 12/12 fail identity transfer with AF/NACK, expected for the absent device |
| All address inventories | Complete; no bus-busy result |
| Controller snapshots | READY; no LISTEN state |

ToF initialization now resets and reassigns all three powered devices
successfully despite their previous addresses being present before the HAL
starts. This differs from the left-ToF boot timeout observed with TCS still
connected. The improvement occurred with identical firmware; no initialization
order change or missing-device bypass was introduced for this comparison.

Three further normal trace bring-up runs completed. The harness verified BUP
FAILED directly in RAM after each run, avoiding dependence on snapshot rows
lost under vendor trace traffic. Each reached the expected absent-TCS identity
failure after ToF initialization. Final read-only flash/RAM capture confirmed
the deployed image matched the trace binary above byte-for-byte and showed:

- IMU and magnetometer online, zero bus errors; cached acceleration
  `(-3.373, 7.885, -5.904) m/s²`.
- All three ToFs online, zero API errors and zero timeouts. Front/right each
  had one accepted sample; left had one rejected out-of-range sample
  (`RangeStatus=4`), rather than an initialization failure.
- I2C1 READY (`0x20`), error code zero after boot snapshot processing.
- TCS phase FAULT and BUP FAILED; application services remain blocked.

Settings sectors remained byte-identical, with the same SHA-256 recorded above.
VS Code reacquired ST-Link after the trace trials. The final capture used its
existing OpenOCD server without halting, resetting, or consuming RTT output.
The target still matched the diagnostic trace image. No additional physical
power cycle was performed for this disconnected-TCS comparison.

The observations point toward a condition associated with the connected TCS
module, cabling, or power as a contributor to the intermittent behavior. They
do not distinguish those causes or establish POR reliability. The earlier
software-reset ToF failure is no longer reproducing in these trials with TCS
removed. Normal bring-up still requires the color sensor, so its absence
deliberately blocks service startup.

Local evidence: `/tmp/ths-no-tcs-summary.log`,
`/tmp/ths-no-tcs-results.json`, their seventeen per-case logs,
`/tmp/ths-no-tcs-trace-repeat-summary.log`,
`/tmp/ths-no-tcs-trace-repeat-results.json`, the three per-run trace logs,
and `/tmp/ths-no-tcs-final-state.json` decoded from the matching flash/RAM
captures. The first full inventory is in `/tmp/ths-no-tcs-00-case-9.log`.

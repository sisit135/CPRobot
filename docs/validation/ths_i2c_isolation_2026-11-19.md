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

Live comparison of the polling fix is pending exclusive ST-Link access; a VS
Code Cortex-Debug session acquired the probe before deployment. No post-fix
hardware success or cold-start qualification is asserted by the host results.

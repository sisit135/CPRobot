# TT 1:90 drive reference at 6 V

The supplied robot estimate is **250 g total mass** with **6 V at DRV8833 VM**,
two TT 1:90 gearmotors, and wheels attached directly to the gearbox outputs.
The exact motor SKU is unknown. These calculations combine comparable published
motors to provide provisional PWM/speed values; they are not measurements of
the installed motors.

## Inputs and sources

| Input | Value | Basis |
| --- | ---: | --- |
| Total robot mass | 0.250 kg | User estimate, including both motors |
| Driver input | 6 V | User estimate at VM, before bridge losses |
| Wheel diameter / radius | 65 / 32.5 mm | [4tronix yellow TT wheel](https://shop.4tronix.co.uk/products/65mm-yellow-wheel-for-robot-vehicle) |
| Tire width | 26 mm | [DFRobot FIT0003](https://www.dfrobot.com/product-352.html), provisional typical value |
| Track | 108.35 mm | 190 mm diagonal square side minus tire width; tires flush inside |
| Axle offset from platform center | -24 mm forward axis | User mounting requirement |
| Gearbox ratio | 90:1 internally; output shaft to wheel 1:1 | User specification; no additional transmission |
| Output no-load speed at 6 V | 120 RPM | [Adafruit 3802 TT 1:90](https://www.adafruit.com/product/3802) |
| No-load / stall current at 6 V | 0.100 / 1.0 A per motor | Same Adafruit reference |
| Output stall torque at 6 V | 1.5 kgf cm = 0.14710 N m per motor | [MakerBotics MB-ELC-TTMOTOR-MG90](https://makerbotics.com/product/mb-elc-ttmotor-mg90/) |
| Bridge on-resistance, HS + LS | 0.360 ohm typical at VM=5 V, 25 C | [TI DRV8833 datasheet](https://www.ti.com/lit/ds/symlink/drv8833.pdf) |

The MakerBotics motor also specifies 120 RPM at 6 V and 100 mA no-load current,
but it is not established that either reference is the installed motor. Gear
ratio alone does not establish stall torque or speed. Motor variation, supply
sag, wiring, driver temperature, and load change these estimates. The nominal
65 mm diameter also needs confirmation against the actual compressed tire.

DRV8833's 2 A peak per bridge is a driver rating, not an assumed motor current
or a configured current-regulation threshold. Its continuous rating depends on
package and cooling. The calculation below draws less than 1 A per motor and
assumes no lower board current limit.

## Electrical and mechanical approximation

All angular velocities and torques below refer to the **gearbox output**.
Direct wheel attachment needs no additional factor of 90.

```text
R_m = V_ref / I_stall                       = 6.000 ohm
R_total = R_m + R_bridge                    = 6.360 ohm
omega_0 = 120 * 2*pi/60                     = 12.5664 rad/s
K_e = (V_ref - I_0*R_m) / omega_0           = 0.429718 V/(rad/s)
K_t_output = torque_stall / (I_stall - I_0) = 0.163444 N m/A

I_at_rest(d) = d*VM / R_total
torque_at_rest(d) = max(0, K_t_output * (I_at_rest(d) - I_0))
omega_no_load(d) = max(0, (d*VM - I_0*R_total) / K_e)
v_no_load(d) = wheel_radius * omega_no_load(d)
a_torque_ceiling(d) = 2*torque_at_rest(d) / (wheel_radius * mass)
```

`d` is duty magnitude in [0,1]. This is an averaged-voltage, constant-friction
approximation. PWM ripple, fast-decay/coast behavior, inductance, static friction,
gearbox efficiency variation, and current limiting are not modeled. In
particular, the partial-duty torque calculation is a rough estimate, not a
measured startup curve. The fitted output torque constant already incorporates
the reference gearbox and losses; it is not the bare rotor's SI torque constant.

| Predicted quantity | Current 100% firmware cap | Previous 80% firmware cap |
| --- | ---: | ---: |
| No-load wheel speed | 119.2 RPM / 12.483 rad/s | 92.5 RPM / 9.690 rad/s |
| No-load straight-line speed | 0.406 m/s | 0.315 m/s |
| Standstill current, each motor | 0.943 A | 0.755 A |
| Net standstill output torque, each motor | 0.138 N m | 0.107 N m |
| Combined tire force before slip/inertia | 8.48 N | 6.59 N |
| Torque-based acceleration ceiling | 33.9 m/s^2 | 26.3 m/s^2 |

These acceleration figures ignore tire slip and rotating inertia. For driven
wheels supporting a fraction `lambda` of robot weight, grip imposes
`a <= mu * lambda * g`. As an illustration only, `mu=0.8` and `lambda=1` limit
acceleration to 7.85 m/s^2 before rotating inertia and rolling loss. A caster
reduces the driven-wheel load fraction. Neither friction coefficient nor weight
distribution has been measured.

Including output-referred rotating inertia `J_output` per drive gives the
straight-line relation `a = (2*torque/R - F_roll)/(mass + 2*J_output/R^2)`.
That inertia includes wheel, gearbox and rotor inertia referred through the
90:1 ratio. Consequently an actual peak acceleration cannot be determined
from the supplied mass, voltage, and ratio alone.

## Firmware and host simulation

[`tt90_reference.hpp`](../fw/robot_sf/include/motor_control/tt90_reference.hpp)
holds the provisional reference and calculations. The nominal running conversion
in [`motor_control/config.hpp`](../fw/robot_sf/include/motor_control/config.hpp)
is now:

```text
duty magnitude = 0.106 + 0.0716197 * abs(requested output rad/s)
```

An exact zero request still produces zero duty. The requested speed ceiling is
12.483 rad/s at the 1.0 duty cap, increased from 9.690 rad/s at the previous
0.8 cap. A 0.04 m/s straight request corresponds
to about 19.4% duty under this reference. The offset estimates running friction;
it does not establish actual breakaway duty. Left/right and forward/reverse
coefficients still need identification on the assembled robot. Control remains
open loop with no encoder speed feedback or voltage compensation.

The navigation task requests up to 0.21 m/s straight travel (about 56.9% duty
for this wheel) and 1.8 rad/s body pivots. The measured distance approach gain is
4/s. These task limits remain below the motor reference ceiling; lifecycle
qualification records establish the results for each tested source revision.
The [v2 cap comparison](validation/platform_v2_speed_2026-11-15.md) records
the unchanged timing with a PWM-only increase and the gains from navigation caps.

The existing request slew limit remains 20 rad/s^2, equivalent to
**0.65 m/s^2 commanded linear acceleration** for equal wheel requests. This is a
limit on nominal requests, not a measurement or guarantee of physical acceleration.

The legacy host plant uses the same sourced speed range and running-duty offset
instead of 20 rad/s per duty. The default 5% right-side mismatch and 0.15/0.17 s
first-order lags remain explicit simulation assumptions; the lifecycle fixture
uses equal gains and 0.15 s lag on both wheels. A sudden full-duty step in that
balanced plant gives an initial 2.70 m/s^2, or 2.10 m/s^2 at 80% duty, because of
the assumed lag. These are not calculated physical peak accelerations. The
kinematic plant does not simulate torque, current, tire traction, or inertia.
The lifecycle now defaults to a separate [surface contact model](surface_contact_model.md)
that includes those effects with explicit friction and inertia assumptions;
`--surface legacy` selects the plant described in this paragraph.

On 2026-11-03, the native Debug core, simulator, production task adapters,
lifecycle executable, and existing core/task fixtures compiled successfully
from a local copy of the current source. Its byte-hash manifest is retained at
`/tmp/cprobot-tt90-local-source/source-sha256.json`; build outputs are under
`/tmp/cprobot-tt90-local-build`. Subsequent headless/GUI E2E runs, private timing
profiles, and focused native results are recorded in the
[TT90 timeout report](validation/tt90_timeout_e2e.md). Older validation artifacts
describe the preceding speed model. See the consolidated
[build guide](building.md) for compilation and validation commands.

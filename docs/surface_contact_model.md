# Host wheel and surface contact model

The lifecycle simulator defaults to `--surface whiteboard`. This is a planar
rigid-body model of the 250 g robot with 65 mm wheels, TT 1:90 gearmotors and
6 V at the DRV8833. It applies the [provisional motor constants](tt90_drive_model.md)
to signed PWM, then transfers force through two tire contacts. Zero PWM uses
coast mode. Motor current, back-EMF, reflected wheel/rotor inertia, passive
gear loss, tire slip, lateral grip and rolling resistance determine motion.
Physics substeps are at most 1 ms. Simulator truth remains outside the firmware
estimator and controller.

## Parameters and evidence

| Parameter | Default | Basis |
| --- | ---: | --- |
| Mass | 0.250 kg | User estimate |
| Chassis diagonal | 0.190 m | Firmware geometry |
| Wheel radius | 0.0325 m | Provisional yellow TT wheel reference |
| Tire width | 0.026 m | [DFRobot FIT0003](https://www.dfrobot.com/product-352.html), provisional |
| Track | 0.1083503 m | Platform side minus tire width, lateral edges flush |
| Axle X offset | -0.024 m | User mounting requirement |
| Static friction coefficient | 0.9 | Assumed dry rubber/smooth-board scenario |
| Sliding friction coefficient | 0.7 | Assumed dry rubber/smooth-board scenario |
| Rolling resistance coefficient | 0.02 | Illustrative value, requires calibration |
| Available contact load, left/right | 1 / 1 | Equal weight on both drive contacts |
| Equivalent wheel rim mass | 0.010 kg | Unmeasured inertia assumption |
| Motor rotor inertia | 2e-8 kg·m² | Unmeasured, reflected through 90:1 gearing |

Rubber/glass experiments show that friction changes with compound, speed,
roughness and surface films. They do not provide a measured coefficient for
this TT tire on a whiteboard. The defaults above are configurable scenario
assumptions. [Tiwari et al., Rubber Adhesion and Friction](https://www.frontiersin.org/journals/mechanical-engineering/articles/10.3389/fmech.2020.620233/full)

Blickle reports rolling forces of 54 N and 152 N for different solid-rubber
wheel types on a 500 kg trolley with 200 mm wheels. Dividing by weight gives
approximately 0.011 and 0.031. The illustrative default 0.02 lies between these
references; their wheel size and load differ substantially from this robot.
[Blickle pulling and pushing guide](https://www.blickle.com/-/media/project/blickle/global-web/service/mediathek/prospekte/pdf/en/fibel_ziehen_und_schieben_en.pdf?fh=c11b2350dc6f187154e014f41bd24aac)

## Contact and damping

Each wheel has available normal load `N = mass * g / 2 * contact_fraction`.
The solver couples wheel spin, world-frame body velocity and yaw through
longitudinal and lateral contact impulses. A contact sticks when the required
impulse fits within the static friction circle; otherwise sliding force is
bounded by `mu_kinetic * N`. Rolling resistance opposes motion with a bounded
`Crr * N` force. Passive impulses cannot reverse motion or create energy.

Contacts are at body coordinates `(a, ±b)`, with `a=-0.024 m` and
`b=track/2`. Their lateral velocity is `v_lateral + a*omega`. A longitudinal
impulse contributes yaw `-side*b*J_long/I`; a lateral impulse contributes
`a*J_lat/I`. The impulse solve includes cross term `-side*a*b/I`, so lateral
grip and rolling drag couple translation and yaw about the rear axle. The legacy
plant likewise preserves the axle midpoint during an ideal pivot; the platform
center moves. GUI tire outlines use the same geometry. The TT output drives the
wheel directly, with no extra ratio. See [mount validation](validation/mount_offsets_2026-11-08.md).

Contact fraction describes available supported load, not visible rubber contact
area. A zero-contact wheel can spin without accelerating the chassis. An already
moving unsupported chassis retains its world-frame velocity rather than having
its velocity rotated with the wheels. This model has no vertical suspension,
caster geometry, floor compliance or temperature dependence.

For full contact, the assumed static/sliding traction acceleration ceilings are
`mu * g`, approximately 8.83 / 6.86 m/s². These are upper bounds before motor
torque and rotating inertia. At 0.04 m/s, rolling resistance alone would stop
the robot in approximately 0.204 s over 4.1 mm; the modeled passive gear loss
adds damping. This estimate is not a calibrated stopping-distance guarantee.

## Controls and diagnostics

The lifecycle CLI exposes surface selection, friction, rolling resistance and
left/right available contact fractions. The GUI displays these assumptions,
body velocity, yaw rate, wheel slip and longitudinal/lateral tire forces. Use
`--surface legacy` to reproduce the previous first-order kinematic plant.
Existing portable fixtures retain that legacy default unless they explicitly
enable surface dynamics. Commands and prerequisites live in
[building.md](building.md#interactive-feature-lifecycle-test-linux).

The focused `robot_surface` fixture covers rest, absent/unequal contact,
friction bounds, sticking/sliding, lateral damping, coast-down without reversal,
mass sensitivity, deterministic replay and invalid parameters. End-to-end
qualification additionally requires the lifecycle's actual goal, tape,
collision, deadline and stopping checks; a passing physics fixture alone does
not qualify the navigation campaign.

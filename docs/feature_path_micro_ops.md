# FEATURE path micro-operations

This is the projected view of **A-star → turn / advance / settle** in the
[THS initialization flow](fw_ths_architecture.md#initialization-flow).
It expands the FEATURE path engine after a resolved map, trusted pose, READY,
and an explicit goal. FEATURE matching supplies observations; A* selects cells;
`PathExecutor` computes body-velocity requests; `path_exec` owns command delivery.
The diagrams describe current source behavior, using the THS application settings.

An editable [Draw.io copy](diagrams/feature_path_micro_ops.drawio) contains these
four diagrams and a linked projection overview. The standalone
[HTML preview](diagrams/feature_path_micro_ops.html) supports page navigation,
pan, zoom and node search. Open the `.drawio` file in Draw.io / diagrams.net to edit.

## Plan a route and select a leg

```mermaid
flowchart TD
  goal["navigation_set_goal(cell) after READY"] --> begin["PathExecutor.begin(): cancel previous route"]
  begin --> valid{"Valid map, fresh tracked pose and executor config?"}
  valid -->|"No"| invalid["FAULT / INVALID_START; zero request"]
  valid -->|"Yes"| astar["A*: fixed 25-cell storage; cardinal known-open edges"]
  astar --> search["Choose lowest g + Manhattan h; relax neighbors; reconstruct source-to-goal cells"]
  search --> route{"Route found?"}
  route -->|"No"| noPath["NO_PATH; zero request"]
  route -->|"Yes, source only"| done["DONE; task still checks end proof"]
  route -->|"Yes, multiple cells"| landmark["next_landmark(): group closed-side straight corridor cells"]
  landmark --> target["Target next side opening, turn or endpoint; retain every route cell"]
  target --> aligned{"Already within 5 degrees of leg heading?"}
  aligned -->|"Yes"| advance["ADVANCE; no initial pivot dwell"]
  aligned -->|"No"| turn["TURN"]
  advance --> tick["Repeated update() decisions below"]
  turn --> tick
```

A* uses cost 10 per edge and `10 × Manhattan distance` as its heuristic;
equal total scores prefer the smaller heuristic. It searches at most 25 nodes.
`begin()` returns a route without commanding hardware. Corridor grouping changes
the observation target, while every traversed edge remains subject to validation.

## Decide one micro-operation per update

`phase_` retains TURN, ADVANCE or SETTLE. `DriveRequest.status` can temporarily
report PAUSED while retaining that phase for the next update. The diagnostic
`ExecuteDecision` names the branch taken; it does not authorize actuation.
Each new tick starts with zero linear and angular velocity, then checks these gates
before entering the phase diagram:

| Check, in source order | Decision / result |
| --- | --- |
| IDLE, DONE, NO_PATH, BLOCKED or FAULT | Return zero; DONE reports `ARRIVED`, others `INACTIVE` |
| Clock before leg start or more than 30 seconds on the leg | `FAULT` / `LEG_TIMEOUT` |
| Driver, motor timing, invalid command or command timeout | `FAULT` / `MOTOR_FAULT` |
| Invalid or stale pose / motor snapshot | `WAIT_POSE` / `WAIT_MOTOR`; PAUSED, zero request |
| Any leg edge no longer known-open | `CLOSED_EDGE`; BLOCKED |
| Observed grid cell outside the current leg | `FAULT` / `UNEXPECTED_CELL` |
| Gates pass | Measure target distance, forward distance, heading error, cross-track error and swept pivot clearance |

Pose age is bounded by 150 ms and motor age by 50 ms. TURN can use a fresh
independent heading (100 ms) with three valid fresh ranges (150 ms) when
translation is unobservable. That exception cannot establish forward motion or
settled arrival without fresh tracked pose evidence.

```mermaid
flowchart TD
  tick["update(): gates above pass"] --> phase{"Retained phase"}
  phase -->|"TURN"| heading{"Within 5 degrees of leg heading?"}
  heading -->|"No"| clear{"Mapped pivot clearance at least 96 mm?"}
  clear -->|"No"| turnWait["WAIT_TURN_CLEARANCE; PAUSED; zero request"]
  clear -->|"Yes"| pivot["TURN_TO_HEADING; v = 0; w = clamp(2 × yaw error)"]
  heading -->|"Yes"| coast["WAIT_TURN_SETTLE; zero request"]
  coast --> turnProof{"400 ms zero PWM + quiet gyro AND 3 fresh stable pose confirmations?"}
  turnProof -->|"No"| coast
  turnProof -->|"Yes"| complete["TURN_COMPLETE; retain ADVANCE for next tick"]

  phase -->|"ADVANCE"| arrival{"Distance at most 10 mm AND corner clearance at least 97 mm, if corner?"}
  arrival -->|"Yes"| stop{"Corner or final endpoint?"}
  stop -->|"Yes"| arrivalStop["ARRIVAL_STOP; zero request; enter SETTLE"]
  stop -->|"No"| passRange{"Fresh front range and no obstacle?"}
  passRange -->|"Yes"| pass["PASS_WAYPOINT; next landmark; cruise without stop"]
  passRange -->|"No"| frontStop["WAIT_FRONT_RANGE / PAUSED or FRONT_BLOCKED / BLOCKED; zero"]
  arrival -->|"No"| recover{"Heading error exceeds 20 degrees?"}
  recover -->|"Yes"| recovery["HEADING_RECOVERY; zero request; enter TURN"]
  recover -->|"No"| passed{"Target overshot beyond allowed corner window?"}
  passed -->|"Yes"| fault["FAULT / TARGET_PASSED; zero request"]
  passed -->|"No"| front{"Fresh front range and no obstacle?"}
  front -->|"No"| frontStop
  front -->|"Yes"| corner{"Corner center reached or passed, but pivot clearance insufficient?"}
  corner -->|"Yes"| turnWait
  corner -->|"No"| drive["ADVANCE / APPROACH_CENTER; measured-distance speed and cross-track steering"]

  phase -->|"SETTLE"| settle["WAIT_ARRIVAL_SETTLE; zero request; detailed decisions below"]
```

The arrows describe decisions across ticks, including waits; they are not a
blocking loop inside `update()`. A clear front range is at least 25 mm and no
older than 100 ms. Pivot clearance measures distance to finite mapped wall
segments; unknown edges count as walls. The swept radius is 95 mm, with a 2 mm
arrival margin and 1 mm hold margin.

During translation, the request is capped at 0.18 m/s. A stopping leg uses
`min(cruise, max(0.003, 4 × forward_distance))` in m/s; a collinear observation
waypoint retains cruise speed. `APPROACH_CENTER` identifies a corner already
within position tolerance but still needing pivot clearance. Steering adds
`atan2(cross_track, max(forward_distance, 0.080))`, limited to ±10 degrees,
to the cardinal heading. Angular requests use twice the heading error, capped
at ±1 rad/s. No nominal velocity is integrated into pose.

## Settle, advance the route, or finish

```mermaid
flowchart TD
  settle["SETTLE: WAIT_ARRIVAL_SETTLE; v = 0; w = 0"] --> drift{"Distance from target exceeds 50 mm?"}
  drift -->|"Yes"| fault["FAULT / SETTLE_DRIFT"]
  drift -->|"No"| clearance{"Corner with mapped pivot clearance below 96 mm?"}
  clearance -->|"Yes"| window{"Still within 10 mm arrival window?"}
  window -->|"Yes"| latch["Keep SETTLE stopped; reset pose confirmations; preserve coast evidence"]
  latch --> settle
  window -->|"No"| reapproach["REAPPROACH_CENTER; enter ADVANCE; reset confirmations and coast"]
  clearance -->|"No"| proof{"400 ms zero PWM + quiet gyro AND 3 fresh stable pose confirmations?"}
  proof -->|"No"| settle
  proof -->|"Yes"| final{"Last route cell?"}
  final -->|"Yes"| arrived["ARRIVED; executor DONE; zero request"]
  final -->|"No"| next["NEXT_WAYPOINT; select next landmark; enter TURN; start new leg deadline"]
```

Coast dwell and pose confirmation count are independent. Gyro must be fresh
(100 ms) and quiet (at most 0.05 rad/s); each confirmation needs a distinct fresh
pose, a new motor snapshot and pose displacement at most 5 mm. Pose jitter resets
confirmations without restarting coast dwell. Nonzero PWM or nonquiet/stale gyro
revokes coast evidence. Low clearance while arrival remains inside tolerance
holds zero output rather than restarting creep; this is the
seed-1485012174 livelock fix (internal record).
Reapproach is specifically the low-clearance corner branch outside that window;
other drift up to 50 mm follows the normal settle proof. Waits consume the
original 30-second leg budget. `PASS_WAYPOINT` and `NEXT_WAYPOINT` start a new
leg deadline; neither extends the shared run budget.

## Project the request into the task and motor boundary

```mermaid
flowchart TD
  observations["FEATURE pose + heading; sensor frame; applied motor snapshot"] --> executor["PathExecutor: route phase + decision -> DriveRequest"]
  executor --> guard["path_exec: optional NAV_TURN_GUARD interceptor"]
  guard -->|"Correction owns motion"| correction["Hold executor updates; bounded posture correction; resume when released"]
  correction --> guard
  guard --> endProof{"Executor DONE?"}
  endProof -->|"Yes"| proof{"Associated end tape and fresh end-center proof?"}
  proof -->|"No"| wait["PAUSED; zero request; await end proof"]
  proof -->|"Yes"| goal["motor_goal_reached(); localization_halt(); deactivate route"]
  endProof -->|"No"| finalCap["Apply fresh target-specific final-approach speed cap when eligible"]
  finalCap --> command["Task publishes WheelCommand; motor_pwm enforces run state and HAL output"]
  wait --> command
  goal --> command
  budget["Shared 86-second run deadline; terminal state / stop enforcement"] -.-> command
```

An unresolved end cue forces zero output and cancels the guard before this
completion check. With the guard enabled, its bounded correction owns motion
and holds FEATURE executor updates; the 30-second leg clock and shared run
deadline continue to elapse. See the
[THS navigation and turn-guard description](fw_ths_architecture.md#geometry-resolution-and-navigation-lifecycle)
for correction details. Executor `v=0` pivot requests pass through this guard:
with a fresh measured pose, it may add at most ±8 mm/s to hold the rear axle
midpoint after proving the remaining mapped arc. The rear-axle geometry and
single-run limitation are recorded in mount validation (internal record).
The final speed cap is
`max(0.005, 2 × (stop_distance − coast_allowance))` m/s when a fresh stop
observation matches the final target. DONE alone cannot declare GOAL; the
deadline wins an arrival at or after 86 seconds. FAULT, BLOCKED and NO_PATH
stop the motor and deactivate the route.

Source evidence:

- [A* search](../fw/robot_sf/src/navigation/AStar.cpp): bounded search, tie-break and route reconstruction.
- [PathExecutor implementation](../fw/robot_sf/src/navigation/path_executor.cpp): `begin`, `next_landmark`, `update`, decisions and geometry.
- [Executor types](../fw/robot_sf/include/navigation/path_executor.hpp): phase/status, faults, decisions and diagnostics.
- [Navigation task](../fw/autonomous/STM32_TINYRCP/src/tasks/navigation_tasks.cpp): application settings, guard ownership, end proof, stop cap and command publication.
- [Turn guard](../fw/robot_sf/include/navigation/turn_guard.hpp): intercepted pivot and correction decisions.

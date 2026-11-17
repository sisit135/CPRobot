# Build and compilation guidelines

This is the canonical build guide for agents and contributors. Run commands from
the repository root unless a command explicitly changes directory. Keep build
outputs outside source directories. Component READMEs describe runtime behavior;
validation reports record results for specific revisions, not current build guarantees.

## Workspace and optional staged builds

The current working folder is no longer on SMB (updated 2026-11-12). Use the
direct build commands below. The staging helper remains optional for other
checkouts or isolated outputs; historical SMB workarounds apply only on SMB.

Run the optional numbered build menu from any working directory:

```sh
python3 /path/to/CPRobot/build_local.py
```

The default staging root is `~/cp_robot_build`. If your user cannot create it,
have it created and assigned to your user once, or choose another local directory
with `--root "$HOME/cp_robot_build"`. Do not run the builds as root. Python 3 and
the selected component's prerequisites below must already be installed.

For the default root, the one-time directory setup on a normal Linux host is:

```sh
sudo install -d -o "$(id -un)" -g "$(id -gn)" /home/cp_robot_build
python3 build_local.py
```

The menu includes THS, ESP32-C6, LuatOS CORE ESP32-C3, Android, native firmware tests, the lifecycle
GUI, standalone navigation, the THS CMSIS and vendor-tracing profiles, and all standard components.
The all option excludes the optional GUI, CMSIS and vendor-tracing profiles. ADR is MicroPython
and has no compilation step. Builds never flash hardware or launch the GUI.
Failed builds return to the menu; all-component builds report failures and
continue through the remaining components.

`source/` links to the repository. `workspace/` contains staged PlatformIO
and Android project scaffolds: source/dependency folders are symlinks, while
small build configuration files and the Gradle wrapper are copied locally.
Configuration copies and target defaults refresh each invocation; ESP-IDF `sdkconfig.<environment>` is
seeded once and subsequent generated configuration stays local. Recreate that
local file explicitly if changing the checked-in SDK configuration. Native CMake
builds read through `source/` and write to `build/`. PlatformIO build/library
directories, tool caches, Gradle module outputs and temporary files also stay
under the selected local root. Existing PlatformIO/Gradle package caches are not
copied; the first build may download dependencies again. `PIO`, `CMAKE`, `CTEST`,
and `BUILD_JOBS` can select installed tools and parallelism.

The bridge scaffold links an existing source certificate/key pair. If neither exists,
the canonical certificate helper generates a local pair; keep that local root
to preserve the device identity across builds. Never transfer its private key.

Source edits are immediately visible without copying the source tree. SMB read
latency and missing-include lookup stalls can still occur through symlinks; this
layout addresses output/cache file-mode failures, not all SMB limitations.
Use one builder at a time per local root, and a separate root per source checkout.

For noninteractive use or to inspect commands without compiling:

```sh
python3 build_local.py --component c6 --jobs 4
python3 build_local.py --component c3 --jobs 4
python3 build_local.py --root /tmp/cprobot-build-preview --component all --dry-run
```

`--dry-run` prepares links and local configuration but does not execute build
commands. `--component` exits with a nonzero status when any selected build fails.
`CP_ROBOT_BUILD_ROOT` can override the default root.

## Native firmware tests

Requires CMake >= 3.16, a C/C++17 compiler, Python 3 with the protobuf package,
`protoc`, and a native build tool.
The default scalar build needs no Arduino SDK, CMSIS download, or network.
When several Python installations are present, pass
`-DPython3_EXECUTABLE=/path/to/python-with-protobuf` to CMake so the schema checks
use the interpreter with the required protobuf package. CMake caches that choice.

```sh
bash fw/scripts/validate_host.sh "$PWD/build/host"
ROBOT_SANITIZERS=ON bash fw/scripts/validate_host.sh "$PWD/build/sanitize"
```

The helper configures Debug, builds, and runs CTest from any working directory.
Override `CMAKE`, `CTEST`, and `BUILD_JOBS` as needed (default parallelism: 4).
Set `ROBOT_CLEAN=ON` after restoring cached build outputs or changing shared
struct layouts so every consumer rebuilds. If tracing prevents LeakSanitizer
from reading `/proc`, set `ASAN_OPTIONS=detect_leaks=0`; address and undefined
behavior checks remain enabled.

Concurrent sandbox builds can collide on GNU Make's `/tmp/GMfifo<PID>` jobserver
FIFO. If the build reports `jobserver mkfifo: ... File exists`, GNU Make versions
with pipe-style jobserver support can avoid that collision with:

```sh
cmake --build build/host --parallel 4 -- --jobserver-style=pipe
```

On the mounted workspace, a native compiler can also stall while probing
nonexistent include paths (observed under `third_party/littlefs/bits/`). If this
occurs, stage the current source and dependency files on a local filesystem and
configure a fresh local build against that copy. Moving only the build output
directory leaves compiler include lookups on the mount. Retain the source
revision or a byte-hash manifest with the build so results identify the source
actually compiled.

Equivalent manual commands and focused E2E output:

```sh
cmake -S fw -B build/host -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host --parallel 4
ctest --test-dir build/host --output-on-failure
ctest --test-dir build/host -R robot_e2e -V
bash fw/autonomous/STM32_TINYRCP/test/run_adapter_checks.sh
```

The independent navigation entry point also supports tests; set
`-DBUILD_TESTING=OFF` when only the portable library is needed:

```sh
cmake -S fw/tools/navigation_host -B build/navigation -DCMAKE_BUILD_TYPE=Debug
cmake --build build/navigation --parallel 4
ctest --test-dir build/navigation --output-on-failure
```

Compile each portable implementation once. `fw/robot_sf/include` must stay free
of Arduino/FreeRTOS includes. Never add `test/fakes` to firmware include paths.
Native mocks do not validate the STM32 SDK, interrupts, scheduler, or board.

## Platform geometry selection

V2 is the only supported platform and is selected by default. To build and run
all native E2E and lifecycle checks for the 135 × 110 mm platform:

```sh
cmake -S fw -B build/geometry-v2 -DCMAKE_BUILD_TYPE=Debug -DAXI_PLATFORM_V2=ON
cmake --build build/geometry-v2 --parallel 4
ctest --test-dir build/geometry-v2 --output-on-failure
```

All STM32 profiles select v2. The named profile below selects the same geometry:

```sh
pio run -d fw/autonomous/STM32_TINYRCP -e blackpill_f411ce_v2
```

Selecting `AXI_PLATFORM_PROTOTYPE=1`, `AXI_PLATFORM_V1=1`, or disabling
`AXI_PLATFORM_V2` stops compilation with an explicit deprecation error.
The native platform-selection regression checks these rejection paths.
Lifecycle logs print the selected dimensions, front-ToF mounting coordinates,
track width, and rear envelope before motion. Native validation is simulation;
it does not qualify physical sensor visibility or board operation.

## Validation figures (Jupyter)

The [turn timing notebook](validation_figures/feature_rhr_time_vs_path_length.ipynb)
requires Python 3, NumPy and Matplotlib; use a Jupyter Python kernel to edit and
run its cells. Open it from within this repository and choose Run All. It reads
the retained comparison/retry archives, checks log hashes, and writes PNG/SVG,
turn-observation CSV, fit CSV and extraction audit beside the notebook. It does
not launch mission binaries. The saved notebook includes executed output.
Filters and the sampled-pose path-length definition are documented in its first
cells and [figure notes](validation_figures/README.md).

## Architecture diagrams

The [THS RTOS diagram generator](diagrams/ths_rtos_data_flow.py) requires only
Python 3 and its standard library. Editable source and layout conventions are
in the [diagram editing guide](diagrams/README.md). Run from the repository root:

```sh
python3 docs/diagrams/ths_rtos_data_flow.py
python3 docs/diagrams/ths_rtos_data_flow.py --check
```

The default output is `docs/diagrams/ths_rtos_data_flow.svg`, resolved beside
the script independently of the working directory. Use `--output /tmp/ths.svg`
to generate a separate copy. `--check` writes nothing and exits 1 for missing or
stale output; it verifies reproduction, not firmware facts or visual layout.

Open the SVG in a browser to review changes. If librsvg's `rsvg-convert` is
installed, an optional PNG preview can be rendered without changing the SVG:

```sh
rsvg-convert -w 1410 -o /tmp/ths_rtos_data_flow.png docs/diagrams/ths_rtos_data_flow.svg
```

The [THS HAL wiring sketch](hardware_wiring.md) has a separate standard-library
SVG generator. Its `--check` verifies reproduction, matches drawing pins to the
source defaults and rejects duplicate active/reserved GPIOs. Review electrical
connections and any build-flag overrides separately:

```sh
python3 docs/diagrams/ths_hal_breadboard.py
python3 docs/diagrams/ths_hal_breadboard.py --check
```

## Rolling-turn test modes

`NAV_NONSTOP_TURN` enables rolling turns by default in every firmware variant
and in native builds. Set the existing `-DNAV_NONSTOP_TURN=1` entry in the base
PlatformIO environment to `0` to disable it for all inherited variants, or use
`-DNAV_NONSTOP_TURN=OFF` in CMake. It is independent of `NAV_TURN_GUARD`:
clearance checks and guard recovery may still interrupt motion.

`ROBOT_MAX_SPEED_ROLLING_TEST` remains a separate default-off full-output
experiment. It requires `NAV_TURN_GUARD=0` (`OFF` in CMake), checked by both the
header and CMake. When enabled it selects the aggressive rolling policy
regardless of `NAV_NONSTOP_TURN`.

| Flag | Motion behavior | Normal motor ramp and speed caps | Measured center/pivot clearance |
| --- | --- | --- | --- |
| `NAV_NONSTOP_TURN=1` | Remove turn coast/dwell and intermediate corner settling; add 0.020 m/s forward feed while turning | Retained, including approach and final tape speed limits | Retained; missing evidence still stops motion |
| `ROBOT_MAX_SPEED_ROLLING_TEST=1` | Rolling turns, with both wheel commands scaled together until the faster wheel reaches full output | Ramp and approach slowdown bypassed | Bypassed in the experiment |

Both preserve the existing veering laws, sensor/data and driver stops, endpoint
stopping and the 86-second run budget. Non-stop mode keeps FEATURE's 0.21 m/s
and 1.8 rad/s caps, RHR's 0.10 m/s and 1.0 rad/s caps, and the normal motor slew
limit. Removing dwell does not force motion through a clearance or sensor stop.

Normal hardware profiles inherit non-stop turns with TurnGuard enabled.
The explicit diagnostic profiles disable TurnGuard for isolated comparisons:

```sh
pio run -d fw/autonomous/STM32_TINYRCP -e blackpill_f411ce_v2_nonstop_turn_test
pio run -d fw/autonomous/STM32_TINYRCP -e blackpill_f411ce_v2_rolling_test
```

These commands compile/link only. Navigation logs identify `motion=nonstop_turn`
or `motion=rolling_max` when the path task starts. The existing local key/start
command flow still controls motion. Normal profiles keep full-output scaling off.
Engine handover waits one task frame for fresh RHR input, so observations that
triggered a FEATURE contradiction do not also choose the first fallback turn.

### Native full-output comparison

Build the experiment's selected targets; the standard qualification suite uses
the separate normal build. Select a Python interpreter with protobuf as described
under [native tests](#native-firmware-tests), if needed.

```sh
cmake -S fw -B build/rolling-max/experiment -DCMAKE_BUILD_TYPE=Debug \
  -DAXI_PLATFORM_V2=ON -DNAV_TURN_GUARD=OFF -DROBOT_MAX_SPEED_ROLLING_TEST=ON
cmake --build build/rolling-max/experiment --parallel 4 \
  --target robot_feature_lifecycle robot_motion_experiment_tests
ctest --test-dir build/rolling-max/experiment -R '^robot_motion_experiment$' --output-on-failure
build/rolling-max/experiment/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle \
  --headless --quiet --seed 1234
```

The full-output native experiment defaults to a 3 mm body-envelope inset when checking wall
contacts. `--scrape-mm 0..5` changes that diagnostic tolerance. Contact is counted;
deeper overlap or center crossing of a closed wall still fails the run. This is
an overlap allowance, not a physical wall-contact/friction model. Normal and non-stop-only builds use zero allowance and reject nonzero values.
The body-envelope inset applies only to the simulator.

To reproduce the paired FEATURE/forced-RHR comparison on the supplied maze and
open arena, build a normal control executable and run the comparison driver:

```sh
cmake -S fw -B build/rolling-max/baseline -DCMAKE_BUILD_TYPE=Debug \
  -DAXI_PLATFORM_V2=ON -DNAV_TURN_GUARD=ON \
  -DNAV_NONSTOP_TURN=OFF -DROBOT_MAX_SPEED_ROLLING_TEST=OFF
cmake --build build/rolling-max/baseline --target robot_feature_lifecycle --parallel 4
python3 fw/autonomous/STM32_TINYRCP/test/lifecycle/run_motion_comparison.py \
  --baseline build/rolling-max/baseline/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle \
  --experiment build/rolling-max/experiment/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle \
  --output-dir build/rolling-max/comparison
```

The driver exits successfully when all 40 trial records are valid and paired.
GOAL, collision and timeout counts are separate outcomes in `summary.json`;
completion of the experiment does not qualify the motion policy. See the
[recorded comparison](validation/v2_rolling_max_2026-11-15.md).

### Native non-stop-only comparison

Use the same TurnGuard setting in the control and experiment to isolate the
turn behavior. Both use strict wall contact checks and the normal speed caps.
Select a Python interpreter with protobuf as described under native tests.

```sh
cmake -S fw -B build/nonstop-turn/control -DCMAKE_BUILD_TYPE=Debug \
  -DAXI_PLATFORM_V2=ON -DNAV_TURN_GUARD=OFF \
  -DROBOT_MAX_SPEED_ROLLING_TEST=OFF -DNAV_NONSTOP_TURN=OFF
cmake -S fw -B build/nonstop-turn/experiment -DCMAKE_BUILD_TYPE=Debug \
  -DAXI_PLATFORM_V2=ON -DNAV_TURN_GUARD=OFF \
  -DROBOT_MAX_SPEED_ROLLING_TEST=OFF -DNAV_NONSTOP_TURN=ON
cmake --build build/nonstop-turn/control --target robot_feature_lifecycle robot_motion_experiment_tests --parallel 4
cmake --build build/nonstop-turn/experiment --target robot_feature_lifecycle robot_motion_experiment_tests --parallel 4
ctest --test-dir build/nonstop-turn/experiment -R '^robot_motion_(experiment|build_flags)$' --output-on-failure
python3 fw/autonomous/STM32_TINYRCP/test/lifecycle/run_motion_comparison.py \
  --baseline build/nonstop-turn/control/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle \
  --experiment build/nonstop-turn/experiment/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle \
  --experiment-mode nonstop --baseline-turn-guard 0 \
  --output-dir build/nonstop-turn/comparison
```

See the [non-stop-only results](validation/v2_nonstop_turn_2026-11-15.md).

## Interactive FEATURE lifecycle test (Linux)

The native build includes `robot_feature_lifecycle`, a five-run qualification
campaign, and focused headless fixtures. The optional GUI requires SDL2 development files, Vulkan headers and
loader, and a Vulkan driver with desktop presentation support. Enabling the GUI
downloads Dear ImGui v1.91.9b with a pinned SHA-256; the normal host build remains
offline. A GPU or a software Vulkan implementation can provide the renderer.

```sh
cmake -S fw -B build/lifecycle -DCMAKE_BUILD_TYPE=Debug -DROBOT_LIFECYCLE_GUI=ON
cmake --build build/lifecycle --target robot_feature_lifecycle --parallel 4
build/lifecycle/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle --map open
ctest --test-dir build/lifecycle -R robot_lifecycle --output-on-failure
```

For an offline GUI build, supply a local v1.91.9b tree using
`-DFETCHCONTENT_SOURCE_DIR_IMGUI=/absolute/path/to/imgui`. If Vulkan headers are
outside the SDK search path, set `-DVulkan_INCLUDE_DIR=/absolute/path/to/include`
and, if needed, `-DVulkan_LIBRARY=/absolute/path/to/libvulkan.so`.

The end goal is hardcoded to Q1 cell `(0,0)` and transformed with the selected
map rotation: Q2 `(4,0)`, Q3 `(4,4)`, Q4 `(0,4)`. The
[relocation record](validation/platform_v2_end_00_2026-11-15.md) records the GUI
result and the four failing checks with the new end. Startup draws an entrance and one of all four cardinal headings
independently from the run seed, including headings beyond a legacy POI mask.
The supplied Q1 maze and its rotations are selected by default. Before starting
the robot, the harness validates the chosen map, start, goal, and a known-open
A* route. Impossible starts fail immediately with a typed reason; they are not
rerolled. Startup-only and bounded RHR fixtures do not require a goal route.
Q1 POIs are ordered `(0,4)`, `(4,4)`, `(4,0)` (IDs 0, 1, 2); other quadrants
rotate the same list. All three have known-open routes to the fixed goal, though
sensor signatures can still be unresolved. See the
[POI reconfiguration record](validation/q1_start_pois_2026-11-09.md).
Focused diagnostic fixtures
can override the goal/entrance/heading; those are not qualification campaigns.
Examples preserving the earlier short-route fixture and a negative tape case:

```sh
build/lifecycle/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle --seed 1234 --entrance 2 --heading 2 --goal 4 2
build/lifecycle/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle --headless --seed 1234 --entrance 2 --heading 2 --goal 4 2 --no-tape --expect-failure
```

The FEATURE boundary regression uses the diagnostic Q1 goal `(2,4)`, rotated
with each arena. It crosses both previously stalled corridor boundaries and
turns at `(2,3)`, then requires physical goal/tape proof and a safe stop within
the unchanged 86-second budget. Run all four rotations with:

```sh
ctest --test-dir build/lifecycle -R '^robot_lifecycle_feature_boundary_prefix_q[0-3]$' --output-on-failure
```

This diagnostic prefix is separate from the fixed-end qualification campaign.
The [FEATURE stall investigation](validation/feature_waypoint3_stall_trace.md)
retains debugger stacks captured after more than five simulated seconds of
zero-command pauses, reproduction details, and post-fix results.

The subsequent [TT90 timeout report](validation/tt90_timeout_e2e.md) records
headless/GUI replays with the current motor calibration, the full five-run
campaign, and private timing/speed diagnostics. Those diagnostics do not count
as fixed-goal qualification passes.

The resolver E2E test uses `--startup-only` to stop once FEATURE resolves or
after a 12-second simulated ambiguity window. It reports the selected map/cell,
candidate count, heading, and initial ranges without setting a goal or moving.
`--range-override channel meters` replaces one simulated L/F/R reading to test
rejection of an inconsistent signature. Run the 98-case matrix covering all three POIs, four rotations, four headings,
configured/blind searches, range rejection and repeated signatures with:

```sh
python3 fw/autonomous/STM32_TINYRCP/test/lifecycle/run_resolver_e2e.py \
  build/lifecycle/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle \
  --output build/lifecycle/autonomous/STM32_TINYRCP/test/lifecycle/resolver-e2e/summary.json
ctest --test-dir build/lifecycle -R '^robot_lifecycle_resolver_e2e$' --output-on-failure
```

Startup-only resolver results and the interactive GUI attempt are recorded in
[resolver E2E validation](validation/feature_resolver_e2e.md).

The native lifecycle executable supports a host-test-only `--force-rhr` flag.
It bypasses FEATURE pose qualification to enter the existing right-hand fallback
after the normal motor-ready gate; it does not inject a feature contradiction.
By itself, `--force-rhr` leaves the ordinary full run active until its usual
terminal result. Add `--rhr-test` for a bounded test that requires an initially
untracked blind-mask pose, RHR COAST/DECIDE/TURN/CLEAR/FOLLOW progression,
measured turn and translation, and a safe terminal halt. The bounded mode halts
after at least one second of measured FOLLOW motion and checks the existing
86-second run budget remains unchanged. It is not a goal or route qualification.

```sh
ctest --test-dir build/lifecycle -R '^robot_lifecycle_(force_rhr_q[0-3]|rhr_test_)' --output-on-failure
build/lifecycle/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle \
  --headless --seed 1234 --entrance 0 --heading 2 --variant 0 \
  --variant-mask 15 --force-rhr --rhr-test
```

With a GUI-enabled build, the bounded run also displays the forced mode, reason,
and current RHR phase in the diagnostics window:

```sh
build/lifecycle/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle \
  --seed 1234 --entrance 0 --heading 2 --variant 0 --variant-mask 15 \
  --force-rhr --rhr-test --speed 20 --exit-on-result
```

Omitting `--rhr-test` leaves `--force-rhr` in the ordinary full-run lifecycle:

```sh
build/lifecycle/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle \
  --seed 1234 --entrance 0 --heading 2 --variant 0 --variant-mask 15 \
  --force-rhr --speed 20 --exit-on-result
```

The forced-RHR lifecycle results and GUI runs are recorded in
[forced RHR validation](validation/forced_rhr_e2e.md).

The exhaustive FEATURE/forced-RHR timing diagnostic runs all four rotations,
both entrances, all four Q1-frame headings, five fixed seeds, and configured
versus blind map search (640 fresh processes). It uses full missions, the fixed
end, and the default whiteboard surface. Build `robot_feature_lifecycle` as
above, then run:

```sh
python3 docs/validation/artifacts/feature_rhr_timing_2026-11-04/run_comparison.py \
  build/lifecycle/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle \
  --output-dir build/lifecycle/feature-rhr-timing --jobs 4
```

The runner retains child exit codes, per-run timings, and compressed full logs.
Its exit 0 means the experiment completed with valid scenario metadata; consult
`all_passed` and each child's result in `summary.json` for mission success.
Controlled start overrides make this a diagnostic matrix, separate from the
random-start qualification below. Timing definitions, every paired result, and
the source manifest are in the [timing report](validation/feature_rhr_timing_2026-11-04.md).
Add `--modes rhr` to retry only RHR (320 runs); use a new output directory to
retain the baseline. To retry only the failures from a previous batch, also pass
`--retry-failures path/to/previous/summary.json`. The runner matches the complete
scenario key and excludes previous successes. The
[RHR GDB follow-up](validation/rhr_gdb_fixes_2026-11-04.md) records the passing
single-mission gates and the restricted retries; FEATURE was not rerun in that
earlier follow-up.

### Large reliability campaign

For later runs, the reusable
[batch entry point](../fw/autonomous/STM32_TINYRCP/test/lifecycle/run_reliability_batch.py)
defaults to **128 trials per permutation per mode**, covering all 96 scenarios
and both modes (24,576 missions). Override the count with `--samples N` or
`--per-permutation N`; preview the complete plan with `--dry-run`:

```sh
python3 fw/autonomous/STM32_TINYRCP/test/lifecycle/run_reliability_batch.py \
  build/reliability/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle \
  --output-dir build/reliability-next --dry-run
```

Remove `--dry-run` to launch. The script also accepts `--jobs` and
`--wall-timeout`. Without an output path it creates a timestamped persistent
directory under `build/reliability-runs`; reuse its printed output path to resume.
This future-run default does not alter the completed 512-trial campaign below.

The native lifecycle now draws omitted seeds directly with x86 RDSEED, checks
CPU support, and retries a busy entropy instruction at most 1,024 times.
Unsupported/exhausted RDSEED reports an error; use explicit seed replay on
other hosts. The printed seed remains sufficient to reproduce a run.
The optional host-only quiet flag retains WARN/ERROR and structured final
timing/physical-stop metrics:

```sh
build/lifecycle/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle \
  --headless --quiet --seed 1485012174
```

For 512 paired trials in every rotation/entrance/heading/search-profile
permutation (65,536 full FEATURE/RHR missions), use a Release build and a
persistent output directory:

```sh
cmake -S fw -B build/reliability -DCMAKE_BUILD_TYPE=Release -DROBOT_LIFECYCLE_GUI=OFF
cmake --build build/reliability --target robot_feature_lifecycle --parallel 4
python3 fw/autonomous/STM32_TINYRCP/test/lifecycle/run_reliability.py \
  build/reliability/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle \
  --samples 512 --jobs 12 --output-dir docs/validation/artifacts/reliability-new
```

Each FEATURE process draws an RDSEED seed and its RHR partner replays it.
To replay a FEATURE trial's original seed, append the recorded seed explicitly
to its command; repeating an omitted-seed command intentionally draws again.
Every trial uses a fresh process, fixed end, default whiteboard plant and the
unchanged 86-second run budget. Headless execution already runs virtual time
without wall-clock sleeps; the GUI speed option does not accelerate it further.
The 1 ms integration step remains unchanged.

Run one writer per output directory. Resume the same command with the same
executable/options: committed pairs restore from atomic compressed workspace
checkpoint parts without rerunning. SQLite is only a local cache because this
mounted filesystem does not support its locking reliably. Do not put the
only checkpoint in a temporary directory. A signature rejects a different
executable or sample count. The retained executable archive allows exact
resume even after local build files are removed.

After completion, the runner saves every command, hardware seed, outcome,
physical check and raw output in compressed JSONL plus CSV and summaries.
The [reliability report](validation/reliability_index_2026-11-04.md) defines
the index and Wilson intervals; its linked Matplotlib notebook renders the
new charts and retains the earlier turn scatter. The report generator reads
only completed data and independently verifies each output hash.

Qualification requires at least five distinct seeds and fresh processes, random
entrances/headings, the fixed end, and every run reaching the physical end-cell
and firmware GOAL. The seeded schedule shuffles all four map variants, satisfying
the requirement of at least two distinct variants. Failed draws are never rerolled
or skipped. The runner saves each log and a machine-readable `summary.json`.

```sh
python3 fw/autonomous/STM32_TINYRCP/test/lifecycle/run_campaign.py build/lifecycle/autonomous/STM32_TINYRCP/test/lifecycle/robot_feature_lifecycle --output-dir build/lifecycle/qualification
ctest --test-dir build/lifecycle -R '^robot_lifecycle_campaign$' --output-on-failure
```

Default seeds are `1234, 2, 3, 13, 2026`; `--seeds` accepts five or more distinct
uint32 seeds. `--campaign-seed` changes the shuffled variant schedule.
The runner accepts `--map open` only for the separately identified integration
fixture. It never passes goal, entrance, heading, or expected-failure overrides
to a child. `robot_lifecycle_campaign` is labeled `qualification` in CTest;
its supplied-map failure must remain a failure until every draw succeeds.
The open campaign and the historical short-route cases exercise the harness and
components, and do not establish supplied-map qualification. Latest per-test
PASS/FAIL results live in the separate
[interactive E2E report](validation/interactive_e2e_results_2026-11-02.md).

Use `--headless` to run without a desktop, `--seed N` to replay noise and the
startup draws, `--entrance 0|1|2` or `--heading 0..3` for diagnostic overrides
(E/N/W/S), and `--variant 0..3` to rotate the physical map, entrances, and
Q1-frame goal. The RAM configuration restricts
FEATURE to the chosen arena by default; `--variant-mask 15` enables a blind
four-map search, which can remain ambiguous. `--map open` selects the separate
boundary-only integration fixture without replacing the supplied map.
`--noise sigma_m` controls Gaussian TOF noise. `--nearest-return` selects the
nearest of nine physical rays across 15 degrees; the nominal model measures the
center ray. Both models draw the same 15-degree diagnostic fan.

The lifecycle defaults to `--surface whiteboard`: the 250 g, 6 V motor/contact
model described in [surface_contact_model.md](surface_contact_model.md).
Friction and inertia defaults are scenario assumptions. `--surface legacy`
replays the former kinematic plant. Optional controls are `--friction-static N`,
`--friction-kinetic N`, `--rolling-resistance N`, and
`--contact-fraction LEFT RIGHT` (available load fractions in 0..1).
The focused physics fixture is `ctest --test-dir build/lifecycle -R '^robot_surface$' --output-on-failure`.

### Standalone Linux GUI snapshot

[`prebuilt/lc_gui_snapshot/`](../prebuilt/lc_gui_snapshot/README.md) is a GUI-enabled Release
runtime snapshot for a machine without this source tree. Copy the entire folder
and run `./run.sh`; `./run.sh --headless` uses the same binary without a desktop.
The launcher selects the included loader, glibc, C++ runtime, SDL and Vulkan
loader libraries. The current bundled host runtime requires **x86-64-v3 / AVX2
and Linux 6.1 or newer**; the destination supplies its X11/Wayland session and
Vulkan graphics driver. Runtime usage and exact requirements are in the package
README; input hashes and library versions are in `build-info.json` and
`source-sha256.json`.

If distributing a `lc_gui_snapshot.tar.gz` archive, preserve executable
permissions when packaging `prebuilt/lc_gui_snapshot/`. Extract it on the
destination and launch:

```sh
tar -xzf lc_gui_snapshot.tar.gz
./lc_gui_snapshot/run.sh
```

`validation/results.json` records relocated, source-hidden FEATURE/RHR headless
missions, GUI missions on both Wayland and X11, and a headless mission with the
host `/usr/lib` hidden. These are build-host checks, not a claim that a second
Linux machine or every Vulkan driver was tested. Rebuild with the GUI commands
above when changing source; this folder is a frozen binary snapshot.

### Turn-guard component only

`NAV_TURN_GUARD` is a CMake option, enabled by default. The STM32 base profile
also defines `NAV_TURN_GUARD=1`; set it to `0` to compile out the command guard.
To build and run only the guard component fixtures:

```sh
cmake -S fw -B build/turn-guard -DCMAKE_BUILD_TYPE=Debug -DNAV_TURN_GUARD=ON
cmake --build build/turn-guard --target robot_turn_guard_tests robot_turn_guard_disabled_tests --parallel 4
ctest --test-dir build/turn-guard -R '^robot_turn_guard_' --output-on-failure
```

The seed-specific full-mission regression for the stopped-arrival livelock is
`ctest --test-dir build/lifecycle -R '^robot_lifecycle_settled_corner_livelock$' --output-on-failure`.
Its [validation report](validation/settled_corner_livelock_2026-11-04.md) retains
the exact failing trace and GDB evidence. `robot_navigation` covers the isolated
clearance-wait behavior and unchanged drift/deadline gates.

The fixtures cover both sides of an 8 mm offset and a lateral coast from safe
to unsafe clearance, followed by the original pivot and a physical stop. They
also exercise stale/invalid inputs, unknown map clearance, cancellation, the
15-second correction bound and the compiled-out passthrough. These are component
fixtures with measured pose supplied directly; they do not qualify resolver,
RHR or lifecycle interaction. See [turn-guard validation](validation/turn_guard_component.md).

GUI keys: **R** toggles rays, **P** toggles the reference path and actual trail,
**D** toggles diagnostics, **Space** pauses simulation time, **N** steps 10 ms
while paused, and **Esc** exits. `--speed 1..20` scales playback;
`--exit-on-result` closes after a terminal result. Start a new process for each
boot because production task state and terminal latches persist until reset.
See [THS architecture](fw_ths_architecture.md#interactive-host-lifecycle-harness)
for the execution boundary and success checks.

## STM32F411CE firmware

Requires PlatformIO and its STM32 Arduino toolchain/framework packages. The
configuration selects C++17 and `stm32duino/STM32duino FreeRTOS@^10.3.3`.

```sh
pio run -d fw/autonomous/STM32_TINYRCP -e blackpill_f411ce
pio run -d fw/autonomous/STM32_TINYRCP -e blackpill_f411ce_cmsis
pio run -d fw/autonomous/STM32_TINYRCP -e blackpill_f411ce_telemetry
pio run -d fw/autonomous/STM32_TINYRCP -e blackpill_f411ce_remote
pio run -d fw/autonomous/STM32_TINYRCP -e blackpill_f411ce_trace
```

The `_trace` profile enables all vendor messages (ST ToF, MPU callbacks and
LittleFS). Every profile defaults to TRACE with USB CDC serial diagnostics configured
before the bring-up task. The interactive builder also exposes `_trace` as
option 9 (`--component ths-trace`).

The monotonic clock reserves TIM5 at 1 MHz through STM32 LL/CMSIS. Focused host
checks cover APB prescaler/TIMPRE selection, initialization, counter wrap and
interrupt-mask preservation:

```sh
cmake -S fw -B /tmp/cprobot-clock-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/cprobot-clock-tests --target robot_monotonic_clock_tests robot_arduino_hal_tests --parallel 4
ctest --test-dir /tmp/cprobot-clock-tests -R '^robot_(monotonic_clock_|arduino_hal$)' --output-on-failure
```

These use host register doubles; a deployed STM32 image is required to qualify
timer frequency, debug halt behavior and deadline behavior on the board.

All STM32 profiles inherit `PIO_FRAMEWORK_ARDUINO_ENABLE_CDC` from the base
profile. STM32duino maps `Serial` to `SerialUSB`, so the existing serial log
sink and deferred stdout/stderr output use the Black Pill's native USB port.
Startup calls `Serial.begin(115200)` without waiting for a host to connect;
115200 is the monitor setting, not a physical USB baud rate. Use a data-capable
USB cable and open the enumerated CDC device (usually `/dev/ttyACM0` on Linux):

```sh
pio device monitor --port /dev/ttyACM0 --baud 115200
```

Replace the port with the actual device. Early records sent before the host
opens the port are not replayed. USB CDC logging does not change the firmware
upload protocol; the default remains ST-Link. USB D−/D+ use PA11/PA12.

Choose the profile required by the change. These commands compile/link only.
Remote configuration, LittleFS, and SPI service support are required in every
profile. `blackpill_f411ce` is the default; the `_telemetry` and `_remote` names
are compatibility aliases with the same required services. Navigation waits
for the local key or a start command, allowing configuration before motion.
Review [hardware wiring](hardware_wiring.md) before any separate flash operation.
On an SMB checkout, PlatformIO library installation can fail while copying
file permissions. Keep its build and dependency directories on a local
filesystem; the source tree can remain mounted:

```sh
PLATFORMIO_BUILD_DIR=/tmp/ths-build PLATFORMIO_LIBDEPS_DIR=/tmp/ths-libdeps \
  pio run -d fw/autonomous/STM32_TINYRCP -e blackpill_f411ce_telemetry
```

The source script `fw/autonomous/STM32_TINYRCP/scripts/platformio_sources.py`
builds portable sources once; PlatformIO builds project `src/` including the
bundled ST vendor, facade, and port once. Do not add an external ST C core.

The CMSIS profile requires a CMSIS-DSP checkout at `third_party/CMSIS-DSP`, or
`custom_cmsis_dsp_root` in the selected PlatformIO environment pointing to one.
The current script compiles only multiplication and transpose kernels; it does
not automatically obtain `framework-cmsis-dsp`. All profiles require
`fw/robot_sf/src/remote/{config,codec,service}.cpp`, generated catalog/schema
assets, EmbeddedProto, and LittleFS; preflight fails when they are missing.
The linker/application size limit is 262144 bytes. Flash sectors 6–7 hold the
two-block settings filesystem; a runtime guard also rejects application overlap.
The previous three-block/sectors-5–7 store is incompatible with this layout;
non-erased or invalid stores are never automatically formatted or migrated.
See [remote configuration](fw_ths_architecture.md#remote-configuration-and-storage) for runtime details.

### Interactive STM32 debugging in VS Code

Open the repository folder, `CPRobot2.code-workspace`, or `Rb-FW.code-workspace`.
The maintained root [.vscode/launch.json](../.vscode/launch.json) uses
[Cortex-Debug](https://github.com/Marus/cortex-debug) with the PlatformIO-installed
OpenOCD and ARM toolchain under `$HOME/.platformio`. Cortex-Debug 1.12.1 and its
Memory View, Peripheral Viewer and RTOS Views dependencies are installed on this
workstation; the extension recommendation is retained in the workspace. Reload
the VS Code window if the provider is not yet recognized.

Connect ST-Link to SWDIO/PA13, SWCLK/PA14 and common ground, with the target
powered appropriately. Stop any other OpenOCD or PlatformIO debug session before
attaching; only one debugger can own the probe.

1. In **Run and Debug**, select **STM32: Attach ST-Link (current trace firmware)**.
2. Press **F5** to attach and halt at the current instruction. This profile has no
   pre-launch build, firmware download or reset command.
3. Inspect **VARIABLES**, **WATCH**, **CALL STACK**, CPU registers,
   **XPERIPHERALS**, and **RTOS Views** while paused. Use F10/F11 to step and F5
   to continue; Shift+F5 disconnects. The debugger's Restart action resets the
   target, so use it only when intentionally restarting firmware.

The executable is
`fw/autonomous/STM32_TINYRCP/.pio/build/blackpill_f411ce_trace/firmware.elf`.
It must match the firmware installed on the board. All STM32 profiles now retain
DWARF symbols via `-g3` while keeping release optimization. Some variables may
still be optimized out or functions inlined. Before the RTT sink was added, the trace build with symbols was
byte-for-byte identical to the image captured from the attached board on
2026-11-12; this does not establish a match after subsequent source edits or
firmware uploads.

To rebuild symbols separately, run **Tasks: Run Task → STM32: Build trace with
symbols (no flash)** or the trace build command above. Do not automatically
rebuild a changed ELF before attaching to an older image. For a different
installed profile, update the launch configuration's `executable` to its matching
ELF. The board project's `.vscode/launch.json` remains generated by PlatformIO;
use the named root attach profile rather than editing that generated file.

Useful Cortex-Debug **Debug Console** expressions/commands while paused:

```gdb
p uxCurrentNumberOfTasks
p pxCurrentTCB->pcTaskName
p 'bup::(anonymous namespace)::bup_status'
p 'robot_log::(anonymous namespace)::ring'[1]
x/2wx 0xE000ED28
```

RTOS Views inspects FreeRTOS task lists through GDB debug types. OpenOCD native
RTOS thread switching is intentionally not enabled: the current image does not
export its required `uxTopUsedPriority` helper. GDB's call stack therefore shows
the active CPU context; RTOS Views provides the task table separately.

Configuration/schema, tool paths, DWARF types and source-line information were
checked. Earlier ST-Link checks verified flash `.text` and read typed variables,
but the session later reported an inconsistent PC and HardFault before the USB
connection disappeared. A final healthy attach/step/disconnect cycle remains
unverified until ST-Link is connected again. The debugger configuration does not
correct the previously diagnosed unconfigured motor pins.

### RTT logs over ST-Link / SWD

All STM32 profiles enable the additional `robot_log::RTT` sink through
`AXI_LOG_RTT_ENABLE`. Rebuild and deliberately deploy the new firmware before
using it; an older flashed image has no RTT control block. The normal build
commands above compile only. Updating the ELF alone does not add logging to an
already running board, and the ELF must match the deployed image.

The VS Code attach profile enables Cortex-Debug RTT channel 0, displayed in a
terminal labelled **THS logs**. Press F5 to continue the CPU after attaching;
new records arrive while it runs. The host needs only the existing ST-Link SWD
connection (PA13/PA14 and ground), with no SWO wire or native USB CDC connection.
The optional USB CDC and SPI telemetry sinks remain available in parallel.

This is RAM-based RTT serviced by the OpenOCD GDB server, not ETM instruction
trace, ITM/SWO output or breakpoint-based semihosting. It requires no debug
exception for each message. Cortex-Debug finds the `_SEGGER_RTT` symbol in the
matching ELF and consumes the static upstream ring; `clearSearch` is false so
attaching does not erase the RTT signature or buffered early logs. See the
[Cortex-Debug RTT guide](https://github.com/Marus/cortex-debug/wiki/SEGGER-RTT-support)
and [OpenOCD RTT commands](https://openocd.org/doc-release/html/General-Commands.html#Real-Time-Transfer-_0028RTT_0029).

With a standalone OpenOCD/GDB session instead of Cortex-Debug's automatic RTT
console, the equivalent GDB commands are:

```gdb
monitor rtt setup 0x20000000 0x20000 {SEGGER RTT}
monitor rtt start
monitor rtt server start 19021 0
```

Connect a host TCP client to `localhost:19021` to read the text stream. This is
a separate log stream managed by the same debug server, not GDB's command
console. Use one consumer for the upstream channel. Firmware stores complete
lines in a 2048-byte ring (2047 usable); a full ring drops new lines and never
waits for the debugger. The initial contents survive a late attach until read
or target reset, subject to that fixed capacity. The ring is consumed once and
is not a replay archive. Dropped RTT lines are counted separately by
`hal_log_rtt_dropped()`; they do not affect `robot_log::dropped()` or the other
sinks. Remove the compile definition to omit the RTT storage and implementation.

Focused host verification, including ring wrap/full behavior and logger fanout:

```sh
cmake -S fw -B /tmp/cprobot-rtt-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/cprobot-rtt-tests --target robot_rtt_logging_tests robot_boot_logging_tests --parallel 4
ctest --test-dir /tmp/cprobot-rtt-tests -R '^robot_(rtt_logging|boot_logging)$' --output-on-failure
```

These checks and the STM32 compile/link validate the implementation and control
block ABI; real ST-Link streaming still requires hardware qualification.

## Optional native CMSIS-DSP verification

Obtain CMSIS-DSP v1.16.0, for example in a writable local checkout:

```sh
git clone --depth 1 --branch v1.16.0 https://github.com/ARM-software/CMSIS-DSP.git /tmp/cprobot-CMSIS-DSP
cmake -S fw -B build/cmsis -DCMAKE_BUILD_TYPE=Debug \
  -DLOC_USE_CMSIS_DSP=ON -DCMSIS_DSP_ROOT=/tmp/cprobot-CMSIS-DSP
cmake --build build/cmsis --parallel 4
ctest --test-dir build/cmsis --output-on-failure
python3 fw/autonomous/STM32_TINYRCP/test/compare_backends.py \
  build/host/autonomous/STM32_TINYRCP/test/robot_backend_trace \
  build/cmsis/autonomous/STM32_TINYRCP/test/robot_backend_trace
```

Build the scalar host trace first. The comparison checks four state values and
16 covariance entries over 1,000 steps with 2e-6 absolute tolerance. Native
CMSIS uses `__GNUC_PYTHON__`; never define that host compiler macro on ARM.
Use the same optimization and floating-point ABI when comparing STM32 profiles.
Numerical agreement does not establish board timing or speedup; see
[CMSIS backend details](ekf/cmsis_dsp.md).

## Android field toolkit

Requires JDK 17, Android SDK Platform 36 and Build Tools 35. The project is a
standalone Gradle Android app; no AOSP source-tree toolset is required. Onboarding
uses app-local adapted views and has no setupdesign/setupcompat library modules. The project uses
compileSdk/targetSdk 36; AGP 8.7.3 emits a compatibility warning for compileSdk 36.

```sh
cd generic_field_tool/android
./gradlew :protocol:test :app:testDebugUnitTest :app:lintDebug :app:assembleDebug
```

On an SMB checkout, Gradle output file-mode operations can fail. Build from a
fresh local copy under `/tmp` in that case. Keep machine-local Gradle caches,
Android Studio state, and `local.properties` out of version control. Rebuild
before relying on the retained APK under `android/artifacts/`.

## ADR USB serial CLI

For automatic port detection and upload of the current ADR source, run:

```sh
./fw/manual/upload.sh
```

The zero-argument Bash script works from any current directory. It detects the
observed CH340 USB ID `1A86:7523`, requires exactly one matching port, uploads
`fw/manual/main.py` as project `ADR`, and continues showing serial boot output
until Ctrl-C. The CLI checks the device firmware before uploading. Close the
IDE/mLink serial connection first. The script reuses an available Python with
pyserial; otherwise it creates `/tmp/cprobot-adr-usb` and installs the CLI
requirements there. It does not run control tests.

For the separate minimal EM1 gamepad check (R2 + Ly, ±20 RPM), upload the
sample explicitly instead of using the ADR application upload script:

```sh
python3 fw/manual/tools/cyberpi_cli.py -p /dev/ttyUSB0 upload fw/manual/samples/gamepad_em1.py --project-name EM1_TEST --monitor
```

The [sample notes](../fw/manual/samples/README.md) identify the exact upstream
baseline and controls. This replaces the active uploaded program with the sample.

Requires Python 3 and pyserial 3.5. From the repository root:

```sh
python3 -m venv /tmp/cprobot-adr-usb
/tmp/cprobot-adr-usb/bin/pip install -r fw/manual/tools/requirements.txt
python3 -B fw/manual/tests/test_cyberpi_cli.py
python3 -B fw/manual/tests/test_upload_script.py
python3 -B fw/manual/tests/test_field_tool.py
/tmp/cprobot-adr-usb/bin/python fw/manual/tools/cyberpi_cli.py ports
/tmp/cprobot-adr-usb/bin/python fw/manual/tools/cyberpi_cli.py stub-info
/tmp/cprobot-adr-usb/bin/python fw/manual/tools/cyberpi_cli.py -p /dev/ttyUSB0 info
/tmp/cprobot-adr-usb/bin/python fw/manual/tools/cyberpi_cli.py -p /dev/ttyUSB0 upload fw/manual/main.py --project-name ADR --monitor
/tmp/cprobot-adr-usb/bin/python fw/manual/tools/cyberpi_cli.py -p /dev/ttyUSB0 --capture /tmp/adr-serial.bin monitor
/tmp/cprobot-adr-usb/bin/python fw/manual/tools/cyberpi_cli.py -p /dev/ttyUSB0 exec --code 'print("custom trace")'
```

Replace the example port with the device listed by `ports` (Windows: `COM3`,
etc.). Close any IDE/mLink connection before using the CLI. `upload` writes the
project file and enters upload mode; `monitor` is passive. Ctrl-C stops monitoring.
The CH340 connection can restart CyberPi when opened. Device commands repeat
only the read-only version query during a startup window (at least 45 seconds;
`--timeout` can extend it). `monitor` remains passive and does not query.
Use `--duration SECONDS` on `monitor`, `exec`, or `upload --monitor` for a bounded
capture. Global options (`--port`, `--capture`, `--show-frames`) precede the
subcommand. `--show-frames` displays incoming protocol payloads on stderr.
Firmware behavior, protocol evidence, tracing, loader dependencies, and current
hardware-validation limits are documented in [ADR USB serial](adr_usb_serial.md).

The two host suites above need only the Python standard library. Serial commands
need pyserial. On POSIX with pyserial installed, the CLI suite also runs a real
subprocess/serial-driver exchange against a pseudo-terminal device simulator;
this remains a host check. Source code is not compiled into a firmware image;
`upload` checks UTF-8 and host Python syntax before opening the device.

Golden transfer fixtures can be reproduced using Node.js and the captured
official `meoser.js` revision identified in the protocol evidence:

```sh
node fw/manual/tools/dump_uploader_vectors.cjs /path/to/meoser.js > /tmp/adr-uploader-golden.json
cmp fw/manual/tests/uploader_golden.json /tmp/adr-uploader-golden.json
```

## Manual robot field-tool provider

The firmware-owned provider in `fw/manual/field_tool_provider` is an independent
Android library project. It depends on the generic transport ABI and protocol
module; the generic app does not compile or include this provider.
Requires the Android prerequisites above and Python 3 with protobuf for codec
generation. Only `fw/manual/main.py` is deployed to CyberPi.

```sh
python3 fw/manual/field_tool_provider/tools/generate_codec.py
python3 fw/manual/field_tool_provider/tools/generate_codec.py --check
python3 -B fw/manual/tests/test_field_tool.py
generic_field_tool/android/gradlew -p fw/manual/field_tool_provider testDebugUnitTest lintDebug dexProvider
```

For firmware-only boot/optional-telemetry changes, the host suite above needs
only standard Python (no protobuf or Android SDK). Its active cases include native
speed API calls, synchronized gripper/pitch angle ramps and shoot sequencing,
boot zero ordering, stop/freshness rules,
disabled catapult/field-tool runtime and a simulated stuck display call.
Upload `fw/manual/main.py` through the existing CyberPi upload-mode
workflow; select the host/USB console to inspect `[ADL boot]` progress markers.
The current ADR commissioning profile comments out field-tool and catapult
runtime wiring and uses onboard PID through `mbot2.drive_speed`, capped at
±100 RPM. Neutral-stick Start arms; no deadman hold is required.
To bypass PID, set `PID_BYPASS = True` near the top of `fw/manual/main.py`
before upload; this selects `drive_power` and the separate `MAX_DRIVE_POWER`
percent limit (default 100%). `False` retains onboard PID and `MAX_RPM`.
Restoring the field tool requires restoring its commented implementation and
wiring, not toggling a runtime flag. These markers do not diagnose errors before the file begins executing.

`dexProvider` writes `build/outputs/provider/adl-provider.zip`, containing only
provider DEX bytecode. The app supplies the ABI and protobuf runtime.
On SMB, stage both Android project sources and the provider on a local filesystem,
then point the independent provider at that Android tree:

```sh
/tmp/cprobot-fieldtool-build/gradlew -p /tmp/cprobot-adl-provider-build \
  -PfieldToolAndroidDir=/tmp/cprobot-fieldtool-build testDebugUnitTest lintDebug dexProvider
```

These checks validate host contracts and build artifacts. Live Android DEX
execution, CyberPi Bluetooth timing and physical robot behavior require devices.

## ESP32 multiboard bridge

PlatformIO pins `espressif32@6.10.0` / ESP-IDF 5.4.0. The shared project is
`generic_field_tool/esp32_multiboard`; `esp32c6` selects `esp32-c6-devkitc-1`
(8 MB), and `luatos_esp32c3` selects `airm2m_core_esp32c3` (4 MB, DIO).
Shared NimBLE/WLAN settings live in `sdkconfig.defaults`; target flash defaults
live in `sdkconfig.defaults.esp32c3` and `.esp32c6`.
Use that project configuration rather than substituting the separately installed
ESP-IDF SDK listed in `AGENTS.md`.

```sh
sh fw/telemetry_bridge/tools/generate_cert.sh
pio run -d generic_field_tool/esp32_multiboard -e esp32c6
pio run -d generic_field_tool/esp32_multiboard -e luatos_esp32c3
```

The certificate and key are embedded at build time. Protect
`generic_field_tool/esp32_multiboard/certs/server.key`; transfer only `server.crt` to hosts.
Bridge host checks run from the repository root with:

```sh
bash generic_field_tool/tests/run.sh
sh fw/telemetry_bridge/tests/run.sh
```

See [bridge setup](../fw/telemetry_bridge/README.md) for provisioning and operation.

## Protobuf generation

Run the relevant generators after changing schemas:

```sh
bash fw/regenerate_protobufs.sh
python3 host/tools/generate_schema.py --check
python3 host/tools/generate_python.py
```

The first generates the catalog, descriptors/source pages, and bounded
EmbeddedProto headers together. The second checks that generated schema assets
are current without modifying them; the third regenerates optional Python gRPC
gateway stubs. To regenerate only descriptors/catalog/source pages, run
`python3 host/tools/generate_schema.py`.

`proto/flash_settings.json` is the allowlist for stable flash keys, types, bounds,
factory values, source references, and runtime bindings. The generator produces
`proto/flash_settings.proto`, the C++ catalog/bindings, Android `robot.desc`, and
firmware schema/source blobs. It also keeps the bridge's proto mirrors current.
Generated compile-time checks detect factory-default drift in FEATURE/RHR
headers. Native tests check descriptor/catalog agreement, malformed manifests,
all field pages, chunk bounds, flash round trips, and power-cut recovery.

The EmbeddedProto helper reuses `third_party/EmbeddedProto` without changing
vendor sources and stages its tools in `/tmp/cprobot-embeddedproto-tools`.
Override `EMBEDDEDPROTO_VENV` for persistent storage or `EMBEDDEDPROTO_DIR` for
another source checkout. It handles stale launchers and SMB mode/symlink issues.
`protoc` and the Python protobuf runtime must have compatible versions.
The HAL vendor target links the existing EmbeddedProto runtime.

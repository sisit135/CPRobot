# CPRobot conversation handoff — 2026-11-02

> On `core_fw`, telemetry transports and field tools have been removed. THS
> keeps local flash settings, USB/RTT diagnostics and the debounced LOC key;
> ADR uses its local gamepad. Transport descriptions below are historical.

This note transfers the useful engineering context from the local Codex
conversation. It does not transfer the conversation itself, source changes,
tool credentials, or local build artifacts. No cloud task has been created.

## Starting point

- Local checkout: `/mnt/adl-ws/Rb-FW/CPRobot`.
- Branch observed when preparing this note: `main`.
- HEAD observed: `d89ae52b810b23cfc92286ae82704e5ab0325cd6`.
- The completed work used a checkout with substantial staged, unstaged, and
  untracked changes from several tasks. Preserve those changes; inspect the
  destination working tree before editing. HEAD alone does not identify the
  implemented source state. No commit or push was made by the last task.
- The latest engineering request was completed: “make remote config not
  optional and re-check scheme generation for flash settings.” The next user
  request is the context transfer itself, not another firmware feature.

Read [AGENTS.md](../AGENTS.md), scoped instructions, [architecture](architecture.md),
and the relevant component architecture before making changes. Read
[building.md](building.md) before compiling or validating; keep commands and
workspace workarounds there instead of creating another build guide.

## User decisions and boundaries

- THS is autonomous STM32F411CE firmware in
  `fw/autonomous/STM32_TINYRCP`, with portable policy in `fw/robot_sf`.
  ADR is manual uPython firmware in `fw/manual`.
- The field tool comprises Android and an ESP32-C6 BLE/WLAN bridge under
  `generic_field_tool`. Do not reintroduce HLCS, UNO R4, or NAN targets.
- Earlier naming requests evolved from RA→ADR to RA→THS for STM32 firmware and
  its docs. Follow the current component terminology, rather than repeating
  historical replacement requests globally.
- EKF development is frozen. Preserve its equations, algorithms, and tuning
  unless the user explicitly authorizes a change; references are in `docs/ekf`.
- Shared logic stays portable. Board operations belong behind HAL; service
  routines consume published sensor samples instead of reading registers.
- Vendor placement/signature rules apply only to embedded BSP/driver code.
  They do not apply to Android, host/UI, or general-purpose dependencies.
- Use existing indentation and `.clang-format`. Follow the repository's commit
  message format if a later task actually requests a commit.
- The user prohibited interactive Android/device/UI testing for this run.
  Automated tests, lint, and builds were performed; no flashing, app installation,
  or robot operation was performed. Retain that boundary until changed.
- Android targets SDK 36. Keep MVVM and onboarding progress/draft state in
  `SavedStateHandle`. Only used SetupWizard-style views/resources remain adapted
  in the app; do not restore whole AOSP setupdesign/setupcompat trees.
- New documentation filenames should use `this-format.md`. Existing canonical
  architecture/build links currently use some older filename styles; do not
  start an unrelated rename sweep.

## Completed mandatory remote configuration work

Every supported THS PlatformIO profile now includes persistence, the protobuf
remote service, and SPI transport. `blackpill_f411ce` is the default; `_telemetry`
and `_remote` are compatibility aliases. The old remote opt-in guard was removed.
Boot requires a valid store before application tasks start. An uncertain flash
commit makes the store unavailable and prevents a later navigation start until
remount/reboot. Start and configuration handling share a mutex; mutation is BUSY
after navigation has started or stop has latched.

`proto/flash_settings.json` is the canonical allowlist for **55 settings**:

| Keys | Meaning |
| --- | --- |
| 100–106, 110–116, 120–126, 130–136 | Retained legacy color slots; storage/calibration only, no mapped floor navigation |
| 200–210 | FEATURE navigation limits and confirmation/contradiction counts |
| 220–232 | Right-hand-rule fallback speeds, clearances, freshness, and timing |
| 240–241 | Black/white tape raw thresholds; zero disables them |
| 250 | Body-Z gyro zero correction |

`host/tools/generate_schema.py` generates and checks the typed
`proto/flash_settings.proto`, C++ catalog and runtime bindings, Android
`robot.desc`, firmware descriptor/source blobs, and bridge proto mirrors.
`FieldSpec` now advertises `default_value` and `source_id`. Generated compile-time
checks catch FEATURE/RHR default and type drift. `--check` fails on stale assets
without changing them. EmbeddedProto and optional Python protobuf outputs were
regenerated as well. EmbeddedProto is reused from its existing checkout;
boilerplate protobuf codecs were replaced in preceding work.

Key implementation paths:

- `fw/robot_sf/include/remote/{catalog_types,catalog,settings,settings_bindings}.hpp`.
- `fw/robot_sf/src/remote/{config,codec,service}.cpp`.
- `fw/autonomous/STM32_TINYRCP/src/remote/flash_store.cpp` and
  `include/remote/flash_layout.hpp`.
- `fw/autonomous/STM32_TINYRCP/scripts/platformio_sources.py`.
- `fw/autonomous/STM32_TINYRCP/test/test_remote_config.cpp` and
  `host/tools/test_generate_schema.py`.

The store validates bounds, finite/integer values, revisions, and cross-field
relationships before committing. Unchanged writes perform no flash mutation.
Updates use a synced temporary file and atomic rename. FEATURE/RHR settings are
captured when navigation starts; tape thresholds are published together and
consumed/reset by the sensor owner; gyro correction is applied in the IMU HAL
before qualification/publication. Gyro calibration consumes 64 distinct fresh
sensor frames with quiet gyro, gravity-magnitude, and variance checks.

Android now uses advertised source IDs and displays factory defaults separately
from saved values. The shared remote decoder is also compiled by the ESP32-C6
bridge. Restore remains a sequence of checked per-field writes with visible
partial failure and no hidden replay or rollback.

## Flash upgrade constraint

The application now reserves sectors 0–5, **256 KiB**, below `0x08040000`.
Settings use sectors 6–7 as two 128 KiB LittleFS blocks. A version-2 record is
248 bytes and fits a 256-byte inline file. Physical flash capacity and linked
application overlap are checked before accessing the filesystem.

This layout and record/catalog format are incompatible with the former
three-block/sectors-5–7 store. An upgrade needs an explicit backup/migration/reset
procedure. That physical migration has **not** been implemented or performed by
this task. Boot never automatically formats a non-erased region or replaces an
invalid record. Build/generation does not erase or migrate a device.

## Previous FEATURE failsafe work to preserve

After initial pose acquisition, three distinct, qualified `NO_CANDIDATE` frames
default to a latched FEATURE contradiction and RHR fallback. Missing/stale or
ambiguous evidence does not count as a contradiction; unresolved startup never
arms RHR. The active route stops before fallback, retains the FEATURE/frozen EKF
observer, preserves sensor/motor guards and the absolute run deadline, and emits
contradiction/entry/phase logs. Cancel and new goals do not clear the latch.
A valid end-tape cue still needs fresh feature evidence for measured final
approach and completion. Preserve the regression cases and logging.

See FEATURE failsafe validation (internal record),
[THS architecture](fw_ths_architecture.md), and [tracing](tracing.md).

## Last recorded validation

The preceding engineering task ran these checks on 2026-11-01; preparing this
handoff did not rerun them or qualify later edits:

- 37/37 scalar native tests and 37/37 ASan/UBSan tests.
- Real LittleFS tests: all 55 field pages, RPC/status/validation, reboot/no-op
  behavior, 16 interruption scenarios over eight observed operation boundaries,
  1,200 commits, and corrupted-record failure without replacement.
- Five Python schema contract cases and current generated-assets check.
- Instrumented adapter/HAL helper: syntax targets built, 3/3 checks passed.
- Generic EmbeddedProto/envelope/SPI/CRC checks and 3 collector/TLS tests passed.
- Android: 27 JVM tests, lint with zero errors/14 warnings, SDK 36 APK assembly,
  and APK signature verification passed.
- STM32 default build: 149,456 / 262,144 bytes flash and 20,624 bytes static RAM.
- ESP32-C6 build: 1,260,801-byte application and 42,980 bytes static RAM.

The detailed evidence, toolchain versions, artifact paths, APK hash, and limits
are in required remote configuration validation (internal record).
Those results are host/build evidence, not physical sensor/transport/motion or
power-loss qualification. The frozen EKF equations and tuning were preserved.

## Destination setup and continuation

Work cloud must be given the actual current source tree as well as this note.
A clean clone at the HEAD above may omit substantial implemented work. Local
absolute paths, `/tmp` artifacts, caches, SDK installations, credentials, and
device access are not automatically available in a cloud environment.

If local computer access is available for the new Work cloud task, select this
same checkout and keep its host connected. Otherwise provide a current source
snapshot or an appropriately published development branch, including generated
and untracked source files. No source upload, commit, push, or account setting
change has been performed by the handoff task.

Start by reading this note and the canonical architecture/build documents,
inspecting the destination Git state, and confirming that the 55-setting catalog
and remote implementation are present. Retain the test/device boundaries above.
Continue with the user's next concrete request; the last engineering task is
already complete.

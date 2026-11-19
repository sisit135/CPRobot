# Core firmware branch

`core_fw` removes the generic Android field tool, ESP32 transport bridges,
ADR field-tool provider and parked BLE runtime, SPI telemetry adapter, Linux
collectors and host gRPC utilities. THS and ADR remain independent firmware projects.

THS keeps its debounced LOC button, local persistent settings, RTT AT access,
USB/RTT logging, FEATURE/RHR control and the frozen EKF. Settings schemas and
generation now belong to `fw/robot_sf`. ADR keeps local gamepad control, actuator
safety and USB upload tools. Its control suite is now `tests/test_control.py`.
Earlier transport diagrams and reports are historical references.

## Validation on 2026-11-20

Commands and prerequisites are in [the build guide](building.md).

- STM32 `blackpill_f411ce` compiles and links: 27,812 bytes RAM and 198,120 bytes flash.
- Native CMake build succeeds; 136 of 154 CTest cases pass. The same 18 failures
  occur on the parent `main` revision (134 of 152 pass). They concern turn-guard,
  startup-turn, lifecycle/RHR/campaign scenarios and the stale HAL wiring drawing.
  The removal introduces no additional failing test names.
- Both added local-start tests pass: debounce/held key and retry after failed start.
- ADR control suite: 67 tests pass. Pending workspace-only tests are excluded.
- Settings schema generation and five contract tests pass; RTT logging and local
  configuration tests pass. The core-only build-menu preview succeeds.

Validation used an isolated copy of the committed source to exclude unrelated
workspace edits. Existing submodule checkouts provided third-party dependencies.
No hardware was flashed and no physical runtime qualification was performed.

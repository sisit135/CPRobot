# Preface
- Original design goals & guidelines defined in separate internal KB

## Documentations

- ESP32 SDK: located at "/mnt/10570F2D10570F2D/GH/esp-idf/"
- [ESP32 API Ref](https://docs.espressif.com/projects/esp-idf/en/v6.1/<target esp>/)

## Architecture and operation

- Read [docs/architecture.md](docs/architecture.md) for the project overview and component boundaries. Keep component layers, initialization, task ownership, operation, and source evidence in [THS firmware architecture](docs/fw_ths_architecture.md), [ADR firmware architecture](docs/fw_adr_architecture.md), according to scope.
- EKF development is currently frozen; references live in [docs/ekf](docs/ekf/README.md). Preserve EKF behavior unless explicitly asked to change it.
- THS targets STM32F411CE. ADR is the manual robot uPython firmware in `fw/manual`. Do not reintroduce UNO R4 or NAN targets.
- This core firmware branch excludes telemetry transports and field tools. Keep firmware settings schemas and generators under their owning `fw/` component.

## Workspace memory

- This working folder is no longer on SMB (updated 2026-11-12). Build directly in this workspace; use the historical SMB staging workaround only on an actual SMB checkout.

## Build and compilation

- Read [docs/building.md](docs/building.md) before compiling or validating changes.
- Keep build prerequisites, commands, profiles, and workspace workarounds in that consolidated guide; link to it instead of duplicating instructions.

## Working with Gits
- Use ONLY this commit message format: "<subsystem>: [n-th minor subcomponent:]:<changes summary>" - e.g., hal / ekf / fbn (feature based navigation)

## Code style
- Follow tabs & indentions in existing files, and adhere to .clang-format preferences

## Implementing the FW
- The vendor placement and signature rules below apply only to third-party BSP and driver code for embedded components. They do not apply to Android, host, UI, or other general-purpose dependencies.
- Embedded BSP and driver vendor files should only be placed in a `vendor/` directory inside their HAL.
- Dont change embedded BSP or driver vendors' function signatures; create a new function w/ required overloads in separate file with suffix.
- Follow the design guide when implementing subsystems
- Sensors / outputs SHOULD be implemented in HAL-style, no readding register in service routine code is allowed

### RTOS
- Use vTask* for implementing another services
- Dont use arduino's setup() - should be leave empty
- Always take target constrains (e.g., RAM / flash) intoconsideration when implementing tasks

## Writing docs
TBD

- Use GFM mermaid charts where suitable.

## Sub-instructions files
- Check scattered "AGENTS.md"s in /fw
- Apply to only scoped directory

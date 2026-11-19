# Robot firmware

THS portable navigation, localization, geometry, and open-loop motor control live
in `robot_sf`; its STM32F411CE HAL and FreeRTOS tasks live in
`autonomous/STM32_TINYRCP`. ADR is the manual robot uPython sketch in `manual`.

See the [project overview](../docs/architecture.md),
[THS architecture](../docs/fw_ths_architecture.md),
[ADR architecture](../docs/fw_adr_architecture.md),
[build guidelines](../docs/building.md), [hardware wiring](../docs/hardware_wiring.md),
and [frozen EKF references](../docs/ekf/README.md).

`core_fw` excludes field tools and telemetry bridges. Firmware settings schemas
and their generator live in `robot_sf/proto` and `robot_sf/tools`.

## Licensing

Project-owned firmware and tests use [GPLv3](../LICENSE.software). Manufacturer dumps
are excluded from public licensing and distribution; third-party code retains
its own notices. See [licensing scope](../LICENSING.md).

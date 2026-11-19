# Architecture

`core_fw` contains the two robot firmware projects and their native development tools.

| Component | Sources | Architecture |
| --- | --- | --- |
| THS | `fw/autonomous/STM32_TINYRCP`, portable core in `fw/robot_sf` | [THS firmware](fw_ths_architecture.md) |
| ADR | `fw/manual` | [ADR firmware](fw_adr_architecture.md) |

THS owns sensor acquisition, localization, FEATURE navigation, RHR fallback,
run deadlines and motor output on STM32F411CE. Successful bring-up starts sensor
polling and the local LOC-key service. Three low samples at 20 ms intervals start
navigation; a failed start can be retried after releasing the key. Boot does not
start a goal. Local flash settings and RTT AT access remain available; settings
writes are blocked after navigation starts. The logger task drains USB/RTT records.
The frozen EKF behavior is preserved; see [EKF references](ekf/README.md).

ADR runs the CyberPi/mBot2 gamepad and actuator program. Its input/control worker
owns outputs; main handles diagnostics. USB upload and calibration utilities remain.

Android field tools, ADR transport providers, ESP32 bridges, SPI telemetry and
host telemetry/gRPC utilities are removed from this branch. Firmware settings
schemas, generated protocol dependencies and their generator live under
`fw/robot_sf`. Earlier diagrams and reports may describe the former integration.

See [building](building.md) for validation commands. Native tests do not qualify
physical hardware, sensor wiring or stopping behavior.

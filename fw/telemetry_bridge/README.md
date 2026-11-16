# ESP32-C6 / LuatOS ESP32-C3 telemetry bridge and Linux collector

The STM32 owns localization/navigation and exchanges bounded SPI cells with
ESP32-C6 / LuatOS ESP32-C3. BLE NUS and WLAN station IPv6/TLS carry the same framed messages to
Android or Linux tools.

See [architecture and operation](../../docs/field_tool_architecture.md#communication-and-command-lifecycle),
[operator workflows](../../docs/field_tool_architecture.md#operator-workflows-and-configuration),
[build guidelines](../../docs/building.md), [hardware wiring](../../docs/hardware_wiring.md),
and the [wire specification](docs/PROTOCOL.md).

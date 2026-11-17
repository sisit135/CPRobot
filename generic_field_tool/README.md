# Generic Field Tool

Android field toolkit and ESP32-C6 / LuatOS ESP32-C3 BLE/WLAN TLS-to-SPI bridge.

- [Field-tool architecture](../docs/field_tool_architecture.md): workflows, bridge tasks, video tracing, and transport limits.
- [Project overview](../docs/architecture.md): THS, ADR, and field-tool boundaries.
- [Build and compilation](../docs/building.md): Android, ESP32-C6 / LuatOS ESP32-C3 and protobuf generation.
- [Android project](android/README.md) and [adapted setup views](android/app/SETUP_UI_SOURCES.md).
- [Envelope schema](protocol.proto) and [shared codecs](common/).

The STM32 telemetry adapter consumes the same generated codec. Optional remote
configuration requires the portable remote implementation described in the
architecture and build guide; generated headers alone do not supply it.

# Third-party dependencies

The STM32 codec uses the pinned [EmbeddedProto](https://github.com/Embedded-AMS/EmbeddedProto)
submodule. LittleFS sources and licenses support the optional remote profile;
other directories retain their own provenance and licenses.

Build and generator instructions live in [docs/building.md](../docs/building.md).
Current runtime/transport boundaries live in [docs/architecture.md](../docs/architecture.md).
The active transport is ESP32-C6 BLE and WLAN/TLS.

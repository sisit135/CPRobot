# Third-party dependencies

The STM32 codec uses the pinned [EmbeddedProto](https://github.com/Embedded-AMS/EmbeddedProto)
submodule. LittleFS sources and licenses support the optional remote profile;
other directories retain their own provenance and licenses.

The optional THS LSM303DLHC backend uses two pinned, unmodified ST submodules:

| Dependency | Pin | Purpose |
| --- | --- | --- |
| [stm32-lsm303dlhc](https://github.com/STMicroelectronics/stm32-lsm303dlhc) | `7fbf7aa073cb1956ba86f5d979632ac483deac0a` (v2.0.2) | BSP acceleration initialization, identity API, register definitions |
| [stm32-bsp-common](https://github.com/STMicroelectronics/stm32-bsp-common) | `80596b4c445333e9379713005d8bcc7f7ef9fc4f` (v5.1.2) | Required `Common/accelero.h` ABI; later releases removed this header |

Both use ST's BSD 3-Clause terms (see the driver `LICENSE.md` and the common
header's license notice). Relative links expose the original files only through
the owning THS HAL's `src/hal/vendor/lsm303dlhc` and `vendor/Common` directories.
Board IO callbacks and burst/conversion extensions live outside vendor code in
`lsm303dlhc_hal.cpp`; vendor signatures and source contents remain unchanged.

Build and generator instructions live in [docs/building.md](../docs/building.md).
Current runtime/transport boundaries live in [docs/architecture.md](../docs/architecture.md).
The active transport is ESP32-C6 BLE and WLAN/TLS.

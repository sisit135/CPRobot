# CyberPi raw Bluetooth API evidence

Artifact: `fw/static_analysis_adr/fw_dump_cyberpi_head/flash.bin`

SHA-256: `aeff183eb0397b15fb13b58d09191377de09736afcfc6d4437dbfdc0f6bf4083`

The OTA_0 application begins at flash offset `0x100000`, size `0x380000`.
Extraction: `/tmp/cprobot-cyberpi-research/cyberpi-ota0.bin`.

## Usable API

```python
import cyberpi

cyberpi.communication.disable_channel_default("bluetooth")
data = cyberpi.communication.read("bluetooth")
cyberpi.communication.send("bluetooth", bytearray(b"reply"))

# Restore built-in parsing when relinquishing this channel:
cyberpi.communication.enable_channel_default("bluetooth")
```

`read` returns a bytearray containing up to 128 available bytes. Maintain an application stream buffer because frame boundaries need not match reads or GATT writes. `send` also accepts bytes, list, tuple, and string; the wrapper converts to bytearray, encoding string as UTF-8.

## Bluetooth GATT UUIDs

Actual firmware attribute table at flash `0x206c78..0x206d08` verifies the standard-base UUIDs:

| Role | UUID | Firmware evidence |
| --- | --- | --- |
| Primary service | `0000ffe1-0000-1000-8000-00805f9b34fb` | 0x2800 declaration at 0x206c78 references service value FFE1 at 0x206d0e |
| Phone writes commands | `0000ffe3-0000-1000-8000-00805f9b34fb` | Characteristic declaration value at 0x206d10 is 0x0c (WRITE and WRITE_NO_RESPONSE); attribute at 0x206ca8 references FFE3 at 0x206d0a; write permission 0x10 and maximum value length 64 |
| Phone receives notifications | `0000ffe2-0000-1000-8000-00805f9b34fb` | Characteristic declaration value at 0x206d11 is 0x10 (NOTIFY); attribute at 0x206cd8 references FFE2 at 0x206d0c; read permission 1 and maximum value length 20 |
| Notification CCCD | `00002902-0000-1000-8000-00805f9b34fb` | Descriptor at 0x206cf0 references 0x2902 at 0x206d12; read/write permissions 0x11 |

Twenty-byte write chunks fit the discovered attribute limits and the default ATT MTU. These are static table observations; connection and notification behavior still need device verification.

## Frozen Python proof

MicroPython opcode layout matches upstream v1.10 `py/bc0.h`, downloaded into `/tmp/cprobot-cyberpi-research/micropython-v1.10` at revision `3e25d611ef3185b68558a20057d50b0d18dc67a0`.

Firmware QSTR tables: native pool pointer entries begin at flash `0x10dbe4`, 3638 entries; frozen pool pointer entries begin at `0x1f6170`, with 3638 preceding entries. QSTR string format is two hash bytes, one length byte, UTF-8 bytes, NUL.

- `driver/communication.py`: source QSTR ID 5293 (`0x14ad`). Frozen module at `0x1dbfc0..0x1dc3e0` imports `communication_o` from `global_objects`.
- At `0x1dc01d..0x1dc031`, channel table maps `"bluetooth"` to `communication_o.PHY_BLE_ID`.
- `send` bytecode at `0x1dc130..0x1dc1ac`: argument QSTR objects preceding code are `channel` and `data` in that order. Validates channel exists. Accepts bytearray/list/tuple/bytes or str; invokes `communication_o.send(channel_table[channel], bytearray(data))`, with UTF-8 for str.
- `read` bytecode at `0x1dc1c4..0x1dc1e3`: argument is channel; invokes `communication_o.recv_raw(channel_table[channel])`.
- `disable_channel_default` bytecode at `0x1dc2b0..0x1dc2f2`: permits uart1/bluetooth/uart2, invokes `communication_o.disable_phy(channel_table[channel])`.
- `enable_channel_default` at `0x1dc2fc..0x1dc33e`: invokes corresponding enable_phy.
- CyberPi module imports `driver.communication` and exposes its `communication` alias at `0x1ec161`.
- `global_objects` initialization at `0x1c72e3..0x1c7304`: under makeblock.COMMUNICATION_ENABLE, imports `communication` from `makeblock`, instantiates with zero arguments, stores as `communication_o`.

## Native proof

Ghidra 12.1.2, Xtensa LE 32-bit, manually mapped ESP image segments without automatic full analysis. Outputs are decompiled static evidence, not hardware observation.

- `native-communication.c`, function `0x40102c5c`: native send accepts `(self, phy_id, bytearray)` and requires exact bytearray type. Bytearray conversion in the Python wrapper is significant.
- `0x40102ca0`: native recv_raw accepts `(self, phy_id)` and reads up to `0x80` bytes into a bytearray.
- `native-communication-underlying.c`, `0x4010a984`: send invokes the physical TX callback directly; does not test parser-enabled state.
- `0x4010aa68`: recv_raw invokes physical RX callback directly.
- `native-communication-flags.c`, `0x40102c2c`: disable_phy sets state 4; enable_phy at `0x40102c44` sets state 3. Neither stops BLE hardware.
- `native-communication-dispatch.c`, `0x4010ab28`: parser refuses physical channels in states 4 or 5, returning `0x103`.
- `native-communication-consumers.c`, BLE callback `0x40101cdc`: incoming channel/service index zero maps to PHY ID 3. Calls parser, and when it returns `0x103`, enqueues bytes into the BLE raw ring buffer. Thus disable_channel_default is the intended handoff to raw reads.

These paths do not invoke reboot, live-mode entry, reset, or uploaded-main cancellation. They hand ownership of the one BLE channel to the custom transport. Built-in commands using that same channel cannot be parsed concurrently; other physical channels and global protocol groups are not disabled.

The native is_phy_enabled/is_protocol_enabled Python wrappers return a constant truth object after querying native state, so they are unsuitable as state assertions on this firmware.

## Loader check

Linked repository: https://github.com/dynacylabs/ghidra-esp32-flash-loader
Checked source revision: `59d88034d07010fb3a3601053554baf02e7be2e4`.
All Java sources compile with `javac --release 21` against installed Ghidra 12.1.2 jars. No extension installation was performed.

The loader checks E9 magic at file offset zero. The supplied full dump has its bootloader at 0x1000, so import the extracted OTA application directly, rather than this unmodified full dump.

Hardware remains untested: live GATT service discovery, notification subscription, sustained stream timing, heap behavior, and radio coexistence must be qualified on the board and phone.

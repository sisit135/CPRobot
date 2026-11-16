# ADL / ADR field-tool provider

This firmware-owned Android DEX provider adapts the manual CyberPi robot to the
platform-agnostic field tool. The Android app has no dependency on this project.
Only `fw/manual/main.py` is uploaded to CyberPi; this directory contains host
development tools and the independently built phone-side provider.

Raw BLE is opt-in: `FIELD_TOOL_ENABLED = False` in `main.py` keeps normal
gamepad operation independent of the field tool. Enable it explicitly before
uploading if this provider is needed. Bluetooth runs in an optional worker;
initialization and I/O errors disable it without exiting the gamepad loop.
Remote ownership is stopped before returning to local input on transport failure;
requests still expire after 200 ms if transport I/O stalls. Cleanup does not
wait for this optional worker. See [firmware runtime](../../../docs/fw_adr_architecture.md)
for boot markers and device-threading limitations.

Import `adl-provider.zip` in the app's Transport provider settings using entry
class `dev.ra.remote.adl.AdlTransportProvider`, select the provider, and enter the
CyberPi BLE address. The provider supplies a synthetic protobuf descriptor with
Arm, Drive, Stop, Grip, Crank, Status and Gamepad request/response pairs.
The app handles those descriptors and bindings without target-specific code.

`schema/commands.json` is the compile-time source for the descriptor, synthetic
`.proto`, Java numeric codec and Python codec block embedded in `main.py`.
The robot needs no protobuf or other codec runtime. Commands are bounded ASCII
numeric fields, versioned and newline framed, with CRC-16/CCITT-FALSE checks.
Phone/firmware tests share golden packets. Generation, host checks and provider
build commands live in the [build guide](../../../docs/building.md#manual-robot-field-tool-provider).

The provider uses the firmware-verified BLE FFE1 service, FFE3 writes and FFE2
notifications with 20-byte chunks. `cyberpi.communication` takes raw ownership
of only the Bluetooth channel, preserving the uploaded application. No live
mode, reset, hidden motion heartbeat or command retry is used.
The Status handshake is read-only. See [firmware API evidence](bluetooth-evidence.md).

The firmware preserves worker-only actuator writes, local e-stop, neutral arm,
deadman, hold-to-run crank control and 200 ms request freshness. Repeated request IDs
return their original admission reply without refreshing motion age. Status
polling does not renew motion. Explicit Gamepad requests stop outputs before
returning input ownership. Responses confirm mailbox admission, not physical
actuator completion; read Status again to observe the current armed state.

For a held drive HUD control, bind `adl.v1.DriveRequest`, use RPM values within
±60 and `deadman=true`, a suitable repeat period (for example 80 ms), and release
parameters with both RPMs zero and `deadman=false`. First tap Arm while neutral.
BLE timing and the existing target freshness window must be qualified on the
device. Hold-repeat sends terminate on app-side errors and never replay after
reconnection.

The same Bluetooth byte pipe cannot concurrently parse mBlock's normal commands.
Its built-in parser is restored on a best-effort basis by the transport worker
during shutdown; a blocked native call can prevent that cleanup. Gamepad
radio coexistence, live GATT behavior, Android ART DEX loading and physical
actuator behavior remain unverified on hardware.

`Grip.port` is a logical mechanism selector: 1 commands the opposing gripper
pair (angle specifies S1; S2 follows its calibrated opposing endpoints), and
2 commands the pitch servo at S3. `Crank.active=true` runs the crank direction
at the selected power, initially 100%; repeat within the freshness window while
held and send `active=false` on release. Retraction and power adjustment are
currently local gamepad controls. The wire schema is unchanged.

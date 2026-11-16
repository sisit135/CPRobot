# Generic Field Tool Android app

The app uses Kotlin MVVM, Compose screens and a small local native setup view.
Only used welcome/content/header/footer layouts are adapted from upstream;
setupdesign/setupcompat library mirrors have been removed.

- [Field-tool architecture and operation](../../docs/field_tool_architecture.md).
- [Project overview](../../docs/architecture.md).
- [Build prerequisites](../../docs/building.md#android-field-toolkit).
- [Setup view provenance](app/SETUP_UI_SOURCES.md).
- [Third-party notices](THIRD_PARTY_NOTICES.md).

This is the initial Android source release (`0.1.0`). See
[release notes](RELEASE_NOTES.md) for its features and validation limits.
Machine-local caches, IDE state, `local.properties`, and built artifacts are
excluded from the source release.

## Transport providers

The app includes the ESP32-C6 BLE/WLAN transport. Settings → Transport provider
also imports a local DEX or ZIP/JAR containing `classes.dex`. The entry class
implements the versioned [TransportProvider ABI](transport-api/src/main/java/dev/ra/remote/transport/TransportProvider.java)
and has a public no-argument constructor. Import copies the code into private,
read-only storage, records its SHA-256, checks that hash before loading, and
uses the app class loader as parent. Import only trusted code: providers execute
with the app's permissions. Schemas remain descriptor data, separate from code.

A provider supplies its own binary `FileDescriptorSet`, including synthetic
request and response types, and a connection that exchanges framed packets.
The app owns request correlation; providers translate envelope kind 9 into their
wire format and translate replies to kind 10. The protocol module and ABI are
host-owned, so provider projects use `compileOnly` for both modules.
Connection methods are blocking and called on I/O threads; `close` must unblock
pending operations. Errors and disconnected operations are never replayed.

Target-specific providers, schemas, codecs, generators and device instructions
belong under the owning firmware component in `fw/`. The generic Android project
does not compile them or assume their entry class, device API or message catalog.
Provider options are JSON string values; common connection settings are passed
as `ble`, `host`, and `pin` options, with explicit provider options taking
precedence. A provider decides which settings it uses.

## Control HUD

The Control tab stores a layout per project. Long press empty space to add a
button or long press a button to configure its label, selectable `*Request`,
parameters, rectangle/rounded/ellipse shape, size and anchors. Enable Edit HUD
to suppress command sends while arranging controls. Tap an anchor value in the
dialog to open the focusable tooltip and key in its exact distance in dp.

Each button stores an independent `(LEFT|RIGHT, offsetDp)` and
`(TOP|BOTTOM, offsetDp)` pair. These refer to physical edges of the available HUD
viewport, inside the app's system-bar/chrome insets; they are not normalized
X/Y percentages. A right/bottom anchored button keeps its edge distance after a
resize or rotation. Off-screen controls can be reopened through the recovery
button in Edit HUD.

Tap bindings send once. Optional hold-repeat bindings require explicit release
parameters for the same request. Repeats stop on release, edit, tab/project
change, backgrounding, disconnect or failure; a connected release is attempted
once and any failure is reported as unconfirmed in Events. Freshness and
actuator policies remain the target's responsibility.

Export/import uses versioned JSON containing all bindings, sizes, shapes and
anchor pairs. Import validates every binding against the selected schema and
replaces the current project's layout only after validation. Other projects
retain their layouts. Limits are 100 controls and a 256 KiB layout.

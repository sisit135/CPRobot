# Initial Android source release — 0.1.0

This release contains the complete standalone Android project: Gradle wrapper
and configuration, app sources/resources, protobuf protocol library, transport
provider ABI, unit tests, licenses and attribution.
Build outputs, APKs, DEX archives, IDE state and local configuration are excluded.
No Git tag or external publication is implied by this source-release commit.

The operator app includes onboarding, project/schema management, sensor and log
views, command responses in Events, remote configuration, and optional video
tracing. Existing ESP32-C6 BLE/WLAN connections remain available alongside
locally imported DEX transport providers. Providers can advertise synthetic
protobuf command catalogs while adapting requests and responses to another
wire format.

The new Control tab binds buttons to schema-defined `*Request` messages.
Layouts persist separately for each project and support independent physical
edge/offset anchors, exact numeric anchoring tooltips, primitive shapes and
button dimensions, long-press creation/editing, and versioned JSON interchange.
Optional hold/release bindings stop when leaving the control surface.

Robot-specific provider implementations, schemas, codecs and generators live
under the owning firmware component and have no compile-time link into this
project. Their DEX files use the same file-import path as third-party providers.
Ordinary Android builds do not require a firmware generator or Python protobuf.
Firmware and broader project documentation are outside this Android-only
source-release commit.

Validation is host-side: Android/protocol unit tests, app lint, debug
APK assembly and APK signature verification. Physical phone UI interaction,
DEX execution on ART, sustained BLE behavior and robot motion
require device qualification; they are not established by the APK build.

See the [initial release validation record](docs/initial-source-release-validation.md)
for exact results and the locally retained APK identity.

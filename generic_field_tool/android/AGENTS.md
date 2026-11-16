# Android field toolkit

- Use Kotlin MVVM: ViewModels own screen state and actions; views bind immutable state and dispatch events. Persist wizard drafts/progress through SavedStateHandle.
- Keep this project platform agnostic and schema driven. New controls and provider loading must work with supplied protobuf descriptors without hardcoded robot message catalogs, target-specific entry classes or compiled provider dependencies. Robot-specific provider implementations, codecs, schema inputs and generators belong under their owning `fw/` directory. Keep existing built-in compatibility isolated; do not require its schema for imported providers.
- For SetupWizard-style UI, adapt only the views/resources actually used. Keep the adapted subset in the app, retain upstream attribution/licenses, and modify it for toolkit needs.
- Do not vendor or mirror entire AOSP setupdesign/setupcompat packages, partner services, AIDL, or unused resources. See [setup view provenance](app/SETUP_UI_SOURCES.md).
- Follow [field-tool architecture and operation](../../docs/field_tool_architecture.md), the [project overview](../../docs/architecture.md), and [build guidelines](../../docs/building.md). Run builds only when authorized and the required toolchain is available.

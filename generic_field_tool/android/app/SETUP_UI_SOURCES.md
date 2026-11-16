# Setup UI sources

Only the used welcome/content/header/footer views are adapted into the app.
There is no vendored setupdesign/setupcompat library or `com.google.android`
package mirror. The view implementation is Kotlin and has no platform setup
service, partner-configuration, AIDL, mixin or provisioning dependency.

| Original | Revision | Retained adaptation |
| --- | --- | --- |
| GrapheneOS SetupWizard2, branch 17 | `3982cfc3536620c17b2ec5c3494af0fb2a58a633` | Welcome/content layout excerpts in `src/main/res/layout/setup_welcome.xml` and `setup_step.xml`; toolkit wording and project input replace device provisioning |
| AOSP setupdesign | `a24fd83ae32682849519ecc14f9bf1a7589f25ff` | Used Glif header/content hierarchy (including `sud_glif_header.xml`) reduced to `setup_wizard_frame.xml` and the app-owned Kotlin `SetupWizardLayout` |
| AOSP setupcompat | `a41906114b2a84df4bde09e9665f794172d8ac25` | Used primary/secondary footer arrangement reduced to ordinary framework buttons in the shared frame |

Source repositories: [GrapheneOS SetupWizard2](https://github.com/GrapheneOS/platform_packages_apps_SetupWizard2/tree/17),
[AOSP setupdesign](https://android.googlesource.com/platform/external/setupdesign),
[AOSP setupcompat](https://android.googlesource.com/platform/external/setupcompat).

The copied views were modified: app resource names and colors replace library
styles; unused progress/profile/partner views were removed; Kotlin binding
replaces mixins; tablet/night resources are local. This is an adapted subset,
not the complete upstream Glif implementation. Copyright headers remain in the
adapted files. GrapheneOS's MIT license and AOSP Apache-2.0 licenses/NOTICE remain
in `src/main/assets/`. The app does not use GrapheneOS branding.

`OnboardingViewModel` owns page/draft state using `SavedStateHandle` and exposes
immutable state. `SetupWizardUi` binds views and dispatches actions. Project
creation/persistence remains in `RemoteModel` / `ProjectStore`.

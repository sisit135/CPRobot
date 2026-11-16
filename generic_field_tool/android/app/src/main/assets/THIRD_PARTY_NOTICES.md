# Third-party notices

Generic Field Tool is distributed as GPL-3.0-only project source. Preserve these notices
and the included licenses when redistributing. Dependency versions below are
pinned declarations; the retained APK dependency snapshot is in docs/android-runtime-dependencies.txt.
That historical snapshot must be regenerated after a future build.

## UI references and adapted code

- **Vector**, JingMatrix and contributors, GNU GPL version 3.
  https://github.com/JingMatrix/Vector/tree/ddeed8ca1ffedfaba7f4341b911089906862d5c4/manager-ui/src/main/kotlin/org/matrix/vector/ui/logs
  Inspected `LogsScreen.kt`, `LogRows.kt`, `LogModel.kt`, `LogContent.kt`.
  The Console follows search, level/tag facets, wrapping, native selectable log
  groups and pause-at-history/follow-at-tail UX. Its robot-specific renderer and
  bounded live buffer are newly implemented; Vector's daemon/window indexer and
  app-specific code are not bundled or claimed to have been ported.
  License: third_party/licenses/vector-GPL-3.0.txt.
- **pixel-volte-patch**, kyujin-cho and contributors, GNU GPL version 3.
  https://github.com/kyujin-cho/pixel-volte-patch/blob/0b4b5fef31e4e4904eece60bdb360ea3111ac3aa/app/src/main/java/dev/bluehouse/enablevolte/pages/Editor.kt
  Config UX uses grouped typed rows, search, sequential fetch/save and loading
  states. `LoadingDialog.kt` is adapted from its components/LoadingDialog.kt at
  the same revision: app-specific resources/previews removed, title/progress
  parameters and bounds added. Attribution is retained in the adapted file.
  License: third_party/licenses/pixel-volte-patch-GPL-3.0.txt.

## Vendored code

- **littlefs 2.9.3**, ARM Limited / littlefs contributors, BSD-3-Clause.
  https://github.com/littlefs-project/littlefs/tree/v2.9.3
  `third_party/littlefs/lfs.c`, `lfs.h`, `lfs_util.c`, `lfs_util.h` are unmodified.
  Original copyright/license header and full license are retained.

## Android libraries and build tools

- AndroidX Activity 1.9.3, Lifecycle 2.8.7 and Compose BOM 2024.12.01:
  The Android Open Source Project / contributors, Apache-2.0.
  https://android.googlesource.com/platform/frameworks/support/
- Kotlin / Compose compiler plugin 2.0.21 and kotlinx-coroutines 1.9.0:
  JetBrains / contributors, Apache-2.0.
  https://github.com/JetBrains/kotlin and https://github.com/Kotlin/kotlinx.coroutines
- Google Protocol Buffers Java + util 4.29.3: Google / contributors, BSD-3-Clause.
  https://github.com/protocolbuffers/protobuf/tree/v29.3
  Gson and Guava are transitive dependencies with their own Apache-2.0 notices.
- Gradle wrapper/distribution 8.11.1: Gradle contributors, Apache-2.0.
  Wrapper JAR obtained from the official Gradle repository.
  https://github.com/gradle/gradle/tree/v8.11.1
- Android Gradle Plugin 8.7.3: The Android Open Source Project, Apache-2.0.
- Test-only JUnit 4.13.2: Eclipse Public License 1.0; Hamcrest: BSD-3-Clause.
  Test-only org.json 20240303 retains the JSON license and is not in the APK.

## Firmware/host dependencies (downloaded by build, not vendored here)

- STM32 Arduino core 3.0.0: LGPL-2.1-or-later and vendor BSD components.
  https://github.com/stm32duino/Arduino_Core_STM32
- STM32duino FreeRTOS 10.3.3 / FreeRTOS: MIT and its accompanying port notices.
  https://github.com/stm32duino/STM32FreeRTOS
- Espressif ESP-IDF 5.4.0 (PlatformIO espressif32 6.10.0): Apache-2.0 plus component-specific notices, including Apache
  NimBLE (Apache-2.0), lwIP (BSD) and Mbed TLS (Apache-2.0).
  https://github.com/espressif/esp-idf
- gRPC Python / grpcio-tools: Apache-2.0; Python protobuf: BSD-3-Clause.
  https://github.com/grpc/grpc and https://github.com/protocolbuffers/protobuf

The baseline RA navigation/EKF/telemetry source comes from the user's prior
project artifacts. SDK sources, production certificates/private keys and host
Python interpreters are not redistributed in this source archive.

## Adapted setup views

- **GrapheneOS SetupWizard2**, Copyright 2023-2025 GrapheneOS, MIT.
  https://github.com/GrapheneOS/platform_packages_apps_SetupWizard2/tree/17
  Revision `3982cfc3536620c17b2ec5c3494af0fb2a58a633`.
  Only the welcome/content view excerpts are adapted for toolkit text, project
  input and Compose paging. The MIT license is bundled as
  `GrapheneOS-SetupWizard2-MIT.txt`.
- **AOSP setupdesign**, Android Open Source Project / Google, Apache-2.0.
  https://android.googlesource.com/platform/external/setupdesign
  Revision `a24fd83ae32682849519ecc14f9bf1a7589f25ff`.
- **AOSP setupcompat**, Android Open Source Project / Google, Apache-2.0.
  https://android.googlesource.com/platform/external/setupcompat
  Revision `a41906114b2a84df4bde09e9665f794172d8ac25`.
  Only the used header/content/footer hierarchy is adapted as app-local XML and
  a small Kotlin view. Library modules, vendor trees, mixins, partner services,
  AIDL and their unused resources have been removed. Adapted files retain
  attribution; Apache licenses and setupdesign NOTICE remain in app assets.
  Full provenance and modifications: `app/SETUP_UI_SOURCES.md`.
  Robolectric 4.14.1 is a test-only dependency (MIT); no tests were run for this edit.

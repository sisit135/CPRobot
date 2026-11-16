// SPDX-License-Identifier: GPL-3.0-only
plugins { id("com.android.library") }
android {
    namespace = "dev.ra.remote.transport"
    compileSdk = 36
    buildToolsVersion = "35.0.0"
    defaultConfig { minSdk = 26 }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
}

// SPDX-License-Identifier: GPL-3.0-only

plugins { id("com.android.application"); id("org.jetbrains.kotlin.android"); id("org.jetbrains.kotlin.plugin.compose") }
android {
    namespace="dev.ra.remote"
    compileSdk=36
    buildToolsVersion="35.0.0"
    defaultConfig { applicationId="dev.ra.remote"; minSdk=26; targetSdk=36; versionCode=1; versionName="0.1.0" }
    buildFeatures { compose=true }
    testOptions { unitTests.isIncludeAndroidResources=true }
    compileOptions { sourceCompatibility=JavaVersion.VERSION_17; targetCompatibility=JavaVersion.VERSION_17 }
    kotlinOptions { jvmTarget="17" }
    packaging { resources.excludes += setOf("META-INF/INDEX.LIST", "META-INF/DEPENDENCIES") }
}
dependencies {
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.robolectric:robolectric:4.14.1")
    testImplementation("org.json:json:20240303")
    implementation(project(":protocol"))
    implementation(project(":transport-api"))
    implementation(platform("androidx.compose:compose-bom:2024.12.01"))
    implementation("androidx.activity:activity-compose:1.9.3")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.material:material-icons-extended")
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.lifecycle:lifecycle-viewmodel-savedstate:2.8.7")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.7")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.7")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.9.0")
}

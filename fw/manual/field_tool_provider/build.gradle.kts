// SPDX-License-Identifier: GPL-3.0-only
plugins { id("com.android.library") version "8.7.3" }
android {
    namespace = "dev.ra.remote.adl"
    compileSdk = 36
    buildToolsVersion = "35.0.0"
    defaultConfig { minSdk = 26 }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    testOptions { unitTests.isReturnDefaultValues = true }
}
dependencies {
    compileOnly(project(":transport-api"))
    compileOnly(project(":protocol"))
    testImplementation(project(":transport-api"))
    testImplementation(project(":protocol"))
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20240303")
}

// Only provider bytecode goes into the DEX. The application supplies the ABI and protobuf runtime.
tasks.register<Exec>("dexProvider") {
    dependsOn("bundleLibRuntimeToJarDebug")
    val output = layout.buildDirectory.dir("outputs/provider")
    inputs.file(layout.buildDirectory.file("intermediates/runtime_library_classes_jar/debug/bundleLibRuntimeToJarDebug/classes.jar"))
    outputs.file(output.map { it.file("adl-provider.zip") })
    doFirst { output.get().asFile.mkdirs() }
    commandLine(
        "${android.sdkDirectory}/build-tools/35.0.0/d8", "--min-api", "26",
        "--lib", "${android.sdkDirectory}/platforms/android-36/android.jar",
        "--output", output.get().file("adl-provider.zip").asFile.absolutePath,
        layout.buildDirectory.file("intermediates/runtime_library_classes_jar/debug/bundleLibRuntimeToJarDebug/classes.jar").get().asFile.absolutePath
    )
}

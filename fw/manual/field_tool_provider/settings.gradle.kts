// SPDX-License-Identifier: GPL-3.0-only
pluginManagement { repositories { google(); mavenCentral(); gradlePluginPortal() } }
dependencyResolutionManagement { repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS); repositories { google(); mavenCentral() } }
rootProject.name = "ADL Field Tool Provider"
val tool = providers.gradleProperty("fieldToolAndroidDir").orNull?.let(::file)
    ?: file("../../../generic_field_tool/android")
include(":transport-api", ":protocol")
project(":transport-api").projectDir = tool.resolve("transport-api")
project(":protocol").projectDir = tool.resolve("protocol")

// SPDX-License-Identifier: GPL-3.0-only

plugins { `java-library` }
java { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
dependencies { api("com.google.protobuf:protobuf-java:4.29.3"); api("com.google.protobuf:protobuf-java-util:4.29.3"); testImplementation("junit:junit:4.13.2") }

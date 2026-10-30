#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
proto = root / "proto"
descriptor = root / "generic_field_tool/android/app/src/main/assets/robot.desc"
subprocess.run([
    "protoc", "-I" + str(proto),
    "--include_imports", "--descriptor_set_out=" + str(descriptor),
    str(proto / "telemetry.proto"), str(proto / "remote_config.proto"),
], check=True)

data = descriptor.read_bytes()
schema = root / "fw/robot_sf/include/remote/schema_blob.hpp"
schema.write_text(
    "// Generated from proto/*.proto. SPDX-License-Identifier: GPL-3.0-only\n"
    "#pragma once\n#include <cstdint>\nnamespace remote {\n"
    "inline constexpr uint8_t schema_blob[]={\n"
    + ",".join(str(byte) for byte in data)
    + "\n};\n}\n"
)

sources = [
    "robot_sf/include/motor_control/localization/config.hpp",
    "robot_sf/include/motor_control/localization/color_tracker.hpp",
    "robot_sf/src/localization/color_tracker.cpp",
]
parts = [
    "// Generated read-only source references. SPDX-License-Identifier: GPL-3.0-only\n"
    "#pragma once\n#include <cstdint>\nnamespace remote {"
]
for index, name in enumerate(sources, 1):
    content = (root / "fw" / name).read_bytes()
    parts.append("inline constexpr uint8_t source_%d[]={%s};" % (
        index, ",".join(str(byte) for byte in content)))
parts.append("}\n")
(root / "fw/robot_sf/include/remote/source_blob.hpp").write_text("\n".join(parts))
print(f"{len(data)} descriptor bytes")

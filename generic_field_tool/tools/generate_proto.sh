#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
command -v protoc >/dev/null
bash "$repo/fw/scripts/setup_embeddedproto.sh"
output="$repo/generic_field_tool/esp32c6/generated"
mkdir -p "$output"
protoc -I "$repo/proto" -I "$repo/generic_field_tool" \
    --plugin="protoc-gen-eams=$repo/generic_field_tool/tools/protoc-gen-eams" \
    --eams_out="$output" telemetry.proto remote_config.proto protocol.proto

resources="$repo/generic_field_tool/android/protocol/src/main/resources"
mkdir -p "$resources"
protoc -I "$repo/generic_field_tool" --include_imports --descriptor_set_out="$resources/field_tool.desc" protocol.proto

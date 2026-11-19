#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Canonical EmbeddedProto generation entry point. No checkout-local setup marker.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
python3 "$repo/fw/robot_sf/tools/generate_schema.py" "$@"
# --check verifies the checked-in catalog/descriptor without rebuilding the plugin.
if [[ "${1:-}" != "--check" ]]; then
    bash "$repo/fw/scripts/setup_embeddedproto.sh"
    protoc -I "$repo/fw/robot_sf/proto" \
        --plugin="protoc-gen-eams=$repo/fw/robot_sf/tools/protoc-gen-eams" \
        --eams_out="$repo/fw/robot_sf/include/remote/protocol" remote_config.proto
fi

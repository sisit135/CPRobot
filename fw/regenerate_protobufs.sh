#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Canonical EmbeddedProto generation entry point. No checkout-local setup marker.
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
exec bash "$repo/generic_field_tool/tools/generate_proto.sh"

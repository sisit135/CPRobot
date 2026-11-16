#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only

set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../../.." && pwd)"
cmake -S "$root" -B "$root/build/host" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$root/build/host" -j
ctest --test-dir "$root/build/host" --output-on-failure

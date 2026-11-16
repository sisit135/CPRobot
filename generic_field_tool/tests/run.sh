#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only

set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
tool="$repo/generic_field_tool"
vendor="$repo/third_party/EmbeddedProto/src"
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/gft-tests.XXXXXX")
cc -std=c11 -Wall -Wextra -Werror -pedantic -c "$repo/fw/telemetry_bridge/common/wire.c" -o "$build_dir/wire.o"
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -I"$vendor" -I"$tool/esp32_multiboard/generated" \
    -I"$tool/common" -I"$tool/esp32_multiboard/src" -I"$repo/fw/robot_sf/include" -I"$repo/fw/telemetry_bridge/common" \
    "$tool/tests/test_protocol.cpp" "$tool/esp32_multiboard/src/protocol_codec.cpp" "$tool/esp32_multiboard/src/remote_codec.cpp" \
    "$vendor/Fields.cpp" "$vendor/MessageInterface.cpp" "$vendor/ReadBufferSection.cpp" "$build_dir/wire.o" -o "$build_dir/protocol_test"
"$build_dir/protocol_test"

#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only

set -eu
cd "$(dirname "$0")"
repo=$(CDPATH= cd -- ../../.. && pwd)
vendor="$repo/third_party/EmbeddedProto/src"
tool="$repo/generic_field_tool"
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -I../common -I"$tool/common" -c test_wire.c -o /tmp/tool_test_wire.o
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -I../common -c ../common/wire.c -o /tmp/tool_wire.o
c++ -std=c++17 -O2 -Wall -Wextra -Werror -pedantic -I"$vendor" -I"$tool/esp32_multiboard/generated" \
    "$tool/common/telemetry_codec.cpp" "$vendor/Fields.cpp" "$vendor/MessageInterface.cpp" "$vendor/ReadBufferSection.cpp" \
    /tmp/tool_test_wire.o /tmp/tool_wire.o -o wire_test
python3 -m unittest -v test_collector.py test_tls.py

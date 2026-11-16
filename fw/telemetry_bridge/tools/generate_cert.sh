#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only

set -eu
repo=$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)
umask 077
mkdir -p "$repo/generic_field_tool/esp32_multiboard/certs"
cd "$repo/generic_field_tool/esp32_multiboard/certs"
if test -e server.key || test -e server.crt; then
    printf '%s\n' 'Certificate already exists; move it aside before regenerating.' >&2
    exit 1
fi
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -sha256 -nodes \
  -days 365 -keyout server.key -out server.crt \
  -subj '/CN=robot-telemetry.local' \
  -addext 'subjectAltName=DNS:robot-telemetry.local' \
  -addext 'basicConstraints=critical,CA:TRUE' \
  -addext 'keyUsage=critical,digitalSignature,keyCertSign' \
  -addext 'extendedKeyUsage=serverAuth'

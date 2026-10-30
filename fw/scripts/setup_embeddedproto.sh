#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
vendor=${EMBEDDEDPROTO_DIR:-"$repo/third_party/EmbeddedProto"}
environment=${EMBEDDEDPROTO_VENV:-"${TMPDIR:-/tmp}/cprobot-embeddedproto-tools"}
# Avoid copied launchers, SMB symlink limitations and the upstream wheel's
# py-modules declaration (the generator is a package, not a single module).
if [[ ! -x "$environment/bin/python" ]]; then
    python3 -m venv --system-site-packages "$environment"
fi
if ! "$environment/bin/python" -c 'import jinja2, markupsafe, six, toposort, google.protobuf' >/dev/null 2>&1; then
    "$environment/bin/python" -m pip install 'Jinja2>=3,<4' 'MarkupSafe>=2,<3' 'six>=1.16,<2' 'toposort>=1.7,<2' protobuf
fi
"$environment/bin/python" - "$vendor/generator" "$environment/generator" <<'PY'
import shutil, sys
shutil.copytree(sys.argv[1], sys.argv[2], dirs_exist_ok=True, ignore=shutil.ignore_patterns('*.egg-info','__pycache__'))
PY
protoc -I "$environment/generator" --python_out="$environment/generator/EmbeddedProto" "$environment/generator/embedded_proto_options.proto"
PYTHONPATH="$environment/generator" "$environment/bin/python" -c 'from EmbeddedProto.main import main_plugin'

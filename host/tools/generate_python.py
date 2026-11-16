#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
proto = root / "proto"
out = root / "host/tools/generated"
out.mkdir(exist_ok=True)
subprocess.run([
    sys.executable, "-m", "grpc_tools.protoc", "-I" + str(proto),
    "--python_out=" + str(out), "--grpc_python_out=" + str(out),
    *[str(path) for path in sorted(proto.glob("*.proto"))],
], check=True)

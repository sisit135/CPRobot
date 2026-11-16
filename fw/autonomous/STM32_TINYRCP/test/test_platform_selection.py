#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compile the supported platform and require explicit v1 deprecation errors."""
from pathlib import Path
import subprocess
import sys

compiler = sys.argv[1]
include = Path(__file__).resolve().parents[3] / 'robot_sf/include'
for flags, supported in (([], True), (['-DAXI_PLATFORM_V2=1'], True),
                         (['-DAXI_PLATFORM_PROTOTYPE=1'], False),
                         (['-DAXI_PLATFORM_V1=1'], False),
                         (['-DAXI_PLATFORM_V2=0'], False)):
    proc = subprocess.run([compiler, '-std=c++17', '-x', 'c++', '-fsyntax-only',
                           '-I', str(include), *flags, '-'],
                          input='#include "platform_geometry.hpp"\n',
                          capture_output=True, text=True)
    assert (proc.returncode == 0) == supported, (flags, proc.stderr)
    if not supported:
        assert 'deprecated' in proc.stderr, proc.stderr
print('PASS default/explicit v2; all v1 selectors fail with deprecation errors')

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compile actual public headers to check hardware motion-mode selections."""
from pathlib import Path
import subprocess
import sys

include = Path(__file__).resolve().parents[3] / 'robot_sf/include'
cases = [([], False, True, None),
         (['NAV_TURN_GUARD=1'], False, True, None),
         (['NAV_TURN_GUARD=0'], False, True, None),
         (['NAV_NONSTOP_TURN=0'], False, False, None),
         (['NAV_NONSTOP_TURN=1', 'NAV_TURN_GUARD=0'], False, True, None),
         (['NAV_NONSTOP_TURN=2'], False, False, 'NAV_NONSTOP_TURN must be 0 or 1'),
         (['ROBOT_MAX_SPEED_ROLLING_TEST=2', 'NAV_TURN_GUARD=0'], False, False,
          'ROBOT_MAX_SPEED_ROLLING_TEST must be 0 or 1'),
         (['ROBOT_MAX_SPEED_ROLLING_TEST=1'], False, False,
          'requires NAV_TURN_GUARD=0'),
         (['ROBOT_MAX_SPEED_ROLLING_TEST=1', 'NAV_TURN_GUARD=1'], False, False,
          'requires NAV_TURN_GUARD=0'),
         (['ROBOT_MAX_SPEED_ROLLING_TEST=1', 'NAV_TURN_GUARD=0'], True, False, None),
         (['ROBOT_MAX_SPEED_ROLLING_TEST=1', 'NAV_TURN_GUARD=0',
           'NAV_NONSTOP_TURN=0'], True, False, None)]
for flags, full, nonstop, error in cases:
    source = '#include "navigation/motion_experiment.hpp"\n'
    source += f'static_assert(nav::motion_test::enabled == {int(full)});\n'
    source += f'static_assert(nav::motion_test::nonstop_only == {int(nonstop)});\n'
    proc = subprocess.run([sys.argv[1], '-std=c++17', '-x', 'c++', '-fsyntax-only',
                           '-I', str(include), '-I', str(include / 'motor_control'),
                           *['-D' + flag for flag in flags], '-'],
                          input=source, capture_output=True, text=True)
    assert (proc.returncode == 0) == (error is None), (flags, proc.stderr)
    if error:
        assert error in proc.stderr, (flags, proc.stderr)
print(f'PASS {len(cases)} hardware motion flag combinations')

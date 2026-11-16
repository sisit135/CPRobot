#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only

"""Compare complete EKF traces from scalar and CMSIS host executables."""
import subprocess
import sys

def trace(executable):
    return [float(v) for v in subprocess.check_output([executable], text=True).split()]

a, b = trace(sys.argv[1]), trace(sys.argv[2])
assert len(a) == len(b) and len(a) > 0
error = max(abs(x-y) for x,y in zip(a,b))
assert error < 2e-6, error
print(f"PASS scalar/CMSIS: {len(a)} state/covariance values; max absolute difference={error:.3g}")

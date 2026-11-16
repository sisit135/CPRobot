#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Run a future FEATURE/RHR batch: 128 trials per permutation by default."""
import argparse
from datetime import datetime, timezone
import os
from pathlib import Path
import shlex
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path, help="Built robot_feature_lifecycle")
    parser.add_argument("--samples", "--per-permutation", type=int, default=128,
                        help="Paired trials per permutation (default: 128)")
    parser.add_argument("--jobs", type=int, default=min(12, os.cpu_count() or 1))
    parser.add_argument("--output-dir", type=Path,
                        help="Persistent results/checkpoints; reuse to resume")
    parser.add_argument("--wall-timeout", type=float, default=45)
    parser.add_argument("--dry-run", action="store_true",
                        help="Print the complete plan and command without launching tests")
    args = parser.parse_args()
    if args.samples < 1 or args.jobs < 1 or args.wall_timeout <= 0:
        parser.error("samples, jobs and wall timeout must be positive")
    executable = args.executable.resolve()
    if not args.dry_run and not executable.is_file():
        parser.error(f"executable does not exist: {executable}")
    runner = Path(__file__).resolve().with_name("run_reliability.py")
    root = runner.parents[5]
    output = args.output_dir
    if output is None:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
        output = root / "build/reliability-runs" / stamp
    output = output.resolve()
    command = [sys.executable, str(runner), str(executable),
               "--samples", str(args.samples), "--jobs", str(args.jobs),
               "--output-dir", str(output), "--wall-timeout", str(args.wall_timeout)]
    print(f"{args.samples} trials per permutation per mode; 96 permutations; "
          f"{96 * args.samples:,} paired trials / {192 * args.samples:,} missions.",
          flush=True)
    print(f"Results and resumable checkpoints: {output}", flush=True)
    print(shlex.join(command), flush=True)
    if args.dry_run:
        return 0
    os.execv(sys.executable, command)


if __name__ == "__main__":
    raise SystemExit(main())

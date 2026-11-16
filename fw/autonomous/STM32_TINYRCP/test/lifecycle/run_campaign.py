#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Qualify five or more fresh FEATURE boots; every child must really reach GOAL."""
import argparse
import json
import math
from pathlib import Path
import random
import re
import subprocess
import sys

Q1_STARTS = ((0, 4), (4, 4), (4, 0))
END_CELL = (0, 0)  # Fixed qualification oracle in the Q1 map frame.
DEFAULT_SEEDS = (1234, 2, 3, 13, 2026)
SCENARIO = re.compile(
    r"SCENARIO seed=(\d+) entrance=(\d+) start=\((\d+),(\d+)\) "
    r"yaw=([-\d.]+) map=Q([1-4]) end=\((\d+),(\d+)\)"
)
DRAW = re.compile(r"draw entrance=random heading=random cardinal=([0-3]) end=fixed")


def rotate(cell, turns):
    x, y = cell
    for _ in range(turns % 4):
        x, y = 4 - y, x
    return x, y


def plan(seeds, campaign_seed):
    rng = random.Random(campaign_seed)
    variants = list(range(4))
    rng.shuffle(variants)
    variants.extend(rng.randrange(4) for _ in range(len(seeds) - 4))
    return list(zip(seeds, variants))


def validate(text, returncode, seed, variant):
    errors = []
    scenario = SCENARIO.search(text)
    draw = DRAW.search(text)
    if scenario is None or draw is None:
        return ["missing randomized scenario/fixed-end metadata"], None
    got_seed, entrance, x, y, yaw, quadrant, gx, gy = scenario.groups()
    metadata = dict(seed=int(got_seed), variant=int(quadrant) - 1,
                    entrance=int(entrance), start=[int(x), int(y)],
                    heading=int(draw[1]), yaw=float(yaw), goal=[int(gx), int(gy)])
    if metadata["seed"] != seed or metadata["variant"] != variant:
        errors.append("scenario does not match the planned seed/variant")
    if metadata["entrance"] not in range(len(Q1_STARTS)):
        errors.append("entrance is outside the startup definition")
    else:
        expected_start = rotate(Q1_STARTS[metadata["entrance"]], variant)
        if tuple(metadata["start"]) != expected_start:
            errors.append("start does not match the drawn entrance")
    if tuple(metadata["goal"]) != rotate(END_CELL, variant):
        errors.append("goal is not the hardcoded end cell transformed with the map")
    angular_error = math.remainder(metadata["yaw"] - metadata["heading"] * math.pi / 2,
                                   2 * math.pi)
    if abs(angular_error) > .001:
        errors.append("starting yaw is not the declared cardinal heading")
    if returncode != 0:
        errors.append(f"child failed with exit code {returncode}")
    if not re.search(r"^PASS: end cell reached AND loc/run GOAL; motors stopped .*state=GOAL", text, re.M):
        errors.append("no successful physical end-cell and GOAL result")
    if re.search(r"^FAIL:", text, re.M):
        errors.append("child reported a failure")
    return errors, metadata


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--seeds", nargs="+", type=int, default=DEFAULT_SEEDS)
    parser.add_argument("--campaign-seed", type=int, default=1234)
    parser.add_argument("--map", choices=("q1", "open"), default="q1")
    parser.add_argument("--output-dir", type=Path, default=Path("build/lifecycle-campaign"))
    args = parser.parse_args()
    if len(args.seeds) < 5 or len(set(args.seeds)) != len(args.seeds):
        parser.error("at least five distinct seeds are required")
    if any(seed < 0 or seed > 0xffffffff for seed in args.seeds):
        parser.error("seeds must fit uint32")
    executable = args.executable.resolve()
    if not executable.is_file():
        parser.error(f"executable does not exist: {executable}")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    schedule = plan(args.seeds, args.campaign_seed)
    results = []
    print(f"CAMPAIGN runs={len(schedule)} fixed_Q1_end={END_CELL} "
          f"map={args.map} campaign_seed={args.campaign_seed} variants={[v for _, v in schedule]}", flush=True)
    for number, (seed, variant) in enumerate(schedule, 1):
        command = [str(executable), "--headless", "--seed", str(seed),
                   "--variant", str(variant), "--map", args.map]
        try:
            child = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, timeout=30)
            output, returncode = child.stdout, child.returncode
        except subprocess.TimeoutExpired as exc:
            output = exc.stdout or b""
            if isinstance(output, bytes):
                output = output.decode(errors="replace")
            output += "\nFAIL: campaign wall-clock watchdog\n"
            returncode = 124
        log = args.output_dir / f"run-{number:02d}-seed-{seed}-q{variant + 1}.log"
        log.write_text(output)
        errors, metadata = validate(output, returncode, seed, variant)
        result = dict(seed=seed, variant=variant, returncode=returncode,
                      passed=not errors, errors=errors, scenario=metadata,
                      command=command, log=str(log.resolve()))
        results.append(result)
        print(f"RUN {number}/{len(schedule)} seed={seed} Q{variant + 1} "
              f"{'PASS' if result['passed'] else 'FAIL'} scenario={metadata} log={log}", flush=True)
        if errors:
            print("  " + "; ".join(errors), flush=True)
            for line in output.splitlines():
                if "reason=" in line or line.startswith(("PASS:", "FAIL:")):
                    print("  " + line, flush=True)
    variants = {r["variant"] for r in results if r["scenario"] is not None}
    coverage_ok = len(variants) == 4  # Stronger than the requested >=2 variants.
    passed = coverage_ok and len(results) >= 5 and all(r["passed"] for r in results)
    summary = dict(passed=passed, map=args.map, fixed_Q1_end=END_CELL,
                   campaign_seed=args.campaign_seed, distinct_variants=sorted(variants),
                   seeds=list(args.seeds), runs=results)
    (args.output_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"CAMPAIGN {'PASS' if passed else 'FAIL'}: "
          f"{sum(r['passed'] for r in results)}/{len(results)} reached GOAL; "
          f"{len(variants)}/4 variants; summary={args.output_dir / 'summary.json'}", flush=True)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())

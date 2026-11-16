#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compare normal and opt-in rolling-turn motion; failures are measured outcomes."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
from statistics import median

from run_campaign import DEFAULT_SEEDS, END_CELL, Q1_STARTS, SCENARIO, DRAW, plan, rotate

FINAL = re.compile(r"^(PASS|FAIL): (.*?) t=([\d.]+) pose=\(([-\d.]+),([-\d.]+)\) state=(\w+) seed=(\d+)$", re.M)


def metrics(output, label):
    match = re.search(r"^" + label + r" (.*)$", output, re.M)
    return dict(re.findall(r"(\w+)=([-\w.]+)", match[1])) if match else {}


def run(executable, profile, arena, mode, seed, variant, log, baseline_guard, experiment_guard):
    command = [str(executable), "--headless", "--quiet", "--seed", str(seed),
               "--variant", str(variant), "--map", arena]
    if mode == "rhr":
        command.append("--force-rhr")
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=30)
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(result.stdout)
    final = FINAL.search(result.stdout)
    scenario = SCENARIO.search(result.stdout)
    draw = DRAW.search(result.stdout)
    physical = metrics(result.stdout, "METRICS")
    motion = metrics(result.stdout, "MOTION_METRICS")
    configuration = metrics(result.stdout, "MOTION_PROFILE")
    errors = []
    if result.returncode not in (0, 1) or not final or not scenario or not draw or not physical or not motion:
        errors.append("missing valid lifecycle result/metadata")
    if configuration.get("rolling_max") != str(int(profile == "rolling")):
        errors.append("motion profile mismatch")
    if configuration.get("nonstop_turn", "0") != str(int(profile == "nonstop")):
        errors.append("nonstop-turn profile mismatch")
    if configuration.get("turn_guard") != str(baseline_guard if profile == "baseline" else experiment_guard):
        errors.append("TurnGuard selection mismatch")
    if physical.get("run_budget_us") != "86000000":
        errors.append("mission budget changed")
    if mode == "rhr" and physical.get("forced_rhr") != "1":
        errors.append("forced RHR was not engaged")
    if scenario and draw:
        got_seed, entrance, x, y, yaw, quadrant, gx, gy = scenario.groups()
        if (int(got_seed), int(quadrant) - 1, (int(gx), int(gy))) != (seed, variant, rotate(END_CELL, variant)):
            errors.append("seed, rotation or fixed goal mismatch")
        if (int(x), int(y)) != rotate(Q1_STARTS[int(entrance)], variant):
            errors.append("start POI mismatch")
    passed = bool(result.returncode == 0 and final and final[1] == "PASS" and final[6] == "GOAL")
    if passed and (physical.get("tape") != "1" or physical.get("zero_pwm") != "1" or
                   physical.get("stopped") != "1" or physical.get("motor_goal") != "1" or
                   physical.get("collision") != "0" or float(physical["end_error_m"]) > .025001):
        errors.append("successful result lacks physical goal/stop proof")
    outcome = "GOAL" if passed else (
        "COLLISION" if physical.get("collision") == "1" else final[6] if final else "PROCESS_ERROR")
    return dict(profile=profile, arena=arena, engine=mode, seed=seed, variant=variant,
                returncode=result.returncode, passed=passed, outcome=outcome,
                finish_s=float(final[3]) if final else None,
                reason=final[2] if final else result.stdout[-400:],
                pose=[float(final[4]), float(final[5])] if final else None,
                scenario=scenario[0] if scenario else None,
                feature_fallback="right-hand fallback: FEATURE" in result.stdout,
                physical=physical, motion=motion, configuration=configuration,
                validation_errors=errors, command=command, log=str(log.resolve()),
                log_sha256=hashlib.sha256(result.stdout.encode()).hexdigest())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--experiment", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--experiment-mode", choices=("rolling", "nonstop"), default="rolling")
    parser.add_argument("--baseline-turn-guard", type=int, choices=(0, 1), default=1)
    parser.add_argument("--experiment-turn-guard", type=int, choices=(0, 1), default=0)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    inputs = {"baseline": args.baseline.resolve(), args.experiment_mode: args.experiment.resolve()}
    binaries = {}
    saved = args.output_dir.resolve() / "executables"
    saved.mkdir(exist_ok=True)
    for profile, source in inputs.items():
        target = saved / profile
        shutil.copy2(source, target)
        binaries[profile] = target
    results = []
    for arena in ("q1", "open"):
        for engine in ("feature", "rhr"):
            for number, (seed, variant) in enumerate(plan(DEFAULT_SEEDS, 1234), 1):
                pair = []
                for profile, executable in binaries.items():
                    log = args.output_dir / profile / arena / engine / f"run-{number:02d}-seed-{seed}-q{variant+1}.log"
                    row = run(executable, profile, arena, engine, seed, variant, log, args.baseline_turn_guard, args.experiment_turn_guard)
                    results.append(row)
                    pair.append(row)
                    print(f"{profile} {arena} {engine} seed={seed} Q{variant+1}: {row['outcome']} peak_pwm={row['motion'].get('peak_pwm')}", flush=True)
                if pair[0]["scenario"] != pair[1]["scenario"]:
                    pair[1]["validation_errors"].append("paired scenarios differ")
                for key in ("wheel_cap_rad_s", "scrape_mm") if args.experiment_mode == "nonstop" else ("wheel_cap_rad_s",):
                    if pair[0]["configuration"].get(key) != pair[1]["configuration"].get(key):
                        pair[1]["validation_errors"].append(f"paired {key} differs")
    groups = []
    for profile in binaries:
        for arena in ("q1", "open"):
            for engine in ("feature", "rhr"):
                rows = [r for r in results if (r["profile"], r["arena"], r["engine"]) == (profile, arena, engine)]
                groups.append(dict(profile=profile, arena=arena, engine=engine,
                                   goals=sum(r["passed"] for r in rows), total=len(rows),
                                   median_goal_s=median(float(r["physical"]["goal_from_key_s"]) for r in rows if r["passed"]) if any(r["passed"] for r in rows) else None,
                                   outcomes=dict(Counter(r["outcome"] for r in rows)),
                                   peak_pwm=max(float(r["motion"].get("peak_pwm", 0)) for r in rows),
                                   moving_ms=sum(int(r["motion"].get("moving_ms", 0)) for r in rows),
                                   full_scale_ms=sum(int(r["motion"].get("full_scale_ms", 0)) for r in rows),
                                   scrape_ms=sum(int(r["motion"].get("scrape_ms", 0)) for r in rows)))
    summary = dict(completed=len(results) == 40 and not any(r["validation_errors"] for r in results),
                   fixed_Q1_end=END_CELL, groups=groups, runs=results,
                   executable_sha256={k: hashlib.sha256(p.read_bytes()).hexdigest() for k, p in binaries.items()})
    (args.output_dir / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(groups, indent=2))
    return 0 if summary["completed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

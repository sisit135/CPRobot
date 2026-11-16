#!/usr/bin/env python3
"""Startup-only E2E checks for FEATURE's blind wall-pattern resolver."""
import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

RESULT = re.compile(
    r"STARTUP_RESULT status=(\w+) resolver=(\d+) kind=(\d+) "
    r"cell=\((-?\d+),(-?\d+)\) candidates=(\d+) "
    r"ranges=\(([-+\d.eE]+),([-+\d.eE]+),([-+\d.eE]+)\) "
    r"heading=([-+\d.eE]+) goal_sent=(\d+) "
    r"duties=\(([-+\d.eE]+),([-+\d.eE]+)\) tracking=(\d+)"
)


def run(binary, args):
    completed = subprocess.run(
        [str(binary), "--headless", "--startup-only", *map(str, args)],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20
    )
    match = RESULT.search(completed.stdout)
    if not match:
        raise RuntimeError(f"missing startup report (rc={completed.returncode}):\n{completed.stdout}")
    names = ("status", "resolver", "kind", "x", "y", "candidates",
             "left", "front", "right", "yaw", "goal_sent", "duty_left",
             "duty_right", "tracking")
    result = dict(zip(names, match.groups()))
    if completed.returncode != 0:
        raise RuntimeError(f"startup child returned {completed.returncode}:\n{completed.stdout}")
    for key in ("resolver", "kind", "x", "y", "candidates", "goal_sent", "tracking"):
        result[key] = int(result[key])
    for key in ("left", "front", "right", "yaw", "duty_left", "duty_right"):
        result[key] = float(result[key])
    result["returncode"] = completed.returncode
    return result, completed.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    cases = []
    failures = []

    # Every ordered Q1 POI, local heading and search profile. A repeated
    # signature must remain stationary; any READY result must be the true
    # rotated map/cell, never a different member of the candidate set.
    starts = ((0, 4), (4, 4), (4, 0))
    for variant in range(4):
        for entrance, cell in enumerate(starts):
            for heading in range(4):
                for profile in ("configured", "blind"):
                    mask = 15 if profile == "blind" else 1 << variant
                    record, _ = run(args.binary, [
                        "--seed", 1234, "--entrance", entrance,
                        "--heading", heading, "--variant", variant,
                        "--variant-mask", mask])
                    record.update(case="startup_permutation", variant=variant,
                                  entrance=entrance, local_heading=heading,
                                  knowledge=profile)
                    cases.append(record)
                    expected_cell = rotate(cell, variant)
                    if record["status"] == "READY":
                        valid = (record["kind"] == variant and
                                 (record["x"], record["y"]) == expected_cell and
                                 record["tracking"] == 1 and is_stationary(record))
                    else:
                        valid = (record["status"] == "RESOLVING" and
                                 record["resolver"] == 3 and
                                 record["candidates"] >= 2 and
                                 record["tracking"] == 0 and is_stationary(record))
                    if not valid:
                        failures.append(f"invalid POI/heading result: {record}")

    bad, _ = run(args.binary, ["--seed", 1234, "--entrance", 2,
                               "--heading", 0, "--variant", 0,
                               "--variant-mask", 15, "--range-override", 1, 1.3])
    bad.update(case="incorrect_front_range", variant=0, local_heading=0)
    cases.append(bad)
    if (bad["status"] != "RESOLVING" or bad["resolver"] != 2 or
            bad["candidates"] != 0 or bad["tracking"] != 0 or
            not is_stationary(bad)):
        failures.append(f"impossible front range was not rejected as NO_CANDIDATE: {bad}")

    repeated, _ = run(args.binary, ["--seed", 1234, "--entrance", 2,
                                    "--heading", 0, "--variant", 0,
                                    "--variant-mask", 15, "--map", "open"])
    repeated.update(case="repeated_signature", variant=0, local_heading=0)
    cases.append(repeated)
    if (repeated["status"] != "RESOLVING" or repeated["resolver"] != 3 or
            repeated["candidates"] < 2 or repeated["tracking"] != 0 or
            not is_stationary(repeated)):
        failures.append(f"repeated wall signature did not remain ambiguous: {repeated}")

    summary = {"cases": cases, "failures": failures,
               "passed": len(cases) - len(failures)}
    rendered = json.dumps(summary, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered)
    print(f"resolver E2E: {summary['passed']}/{len(cases)} checks passed")
    for case in cases:
        print(f"{case['case']}: Q{case['variant'] + 1} local_h={case['local_heading']} "
              f"startup={case['status']} resolver={case['resolver']} "
              f"kind={case['kind']} cell=({case['x']},{case['y']}) "
              f"candidates={case['candidates']}")
    for failure in failures:
        print(f"FAIL: {failure}", file=sys.stderr)
    return 1 if failures else 0


def rotate(cell, variant):
    x, y = cell
    for _ in range(variant):
        x, y = 4 - y, x
    return x, y


def is_stationary(record):
    return (record["goal_sent"] == 0 and abs(record["duty_left"]) < 1e-6 and
            abs(record["duty_right"]) < 1e-6)


if __name__ == "__main__":
    sys.exit(main())

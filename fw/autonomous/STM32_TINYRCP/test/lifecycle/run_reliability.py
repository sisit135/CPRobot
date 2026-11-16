#!/usr/bin/env python3
"""Paired RDSEED reliability matrix; fresh headless processes and virtual time.

Atomic compressed checkpoint parts stay in the chosen output directory; use a
persistent workspace path. SQLite is only a local cache, avoiding mounted-file
locking issues. runs.jsonl.gz preserves every raw output and trial record;
summary.json and results.csv.gz are portable exports.
"""
import argparse
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor, wait, FIRST_COMPLETED
import csv
from datetime import datetime, timezone
import gzip
import hashlib
import itertools
import json
import math
from pathlib import Path
import re
import sqlite3
import subprocess
import tempfile
import time
import zlib

SEED = re.compile(r'^SEED source=(\w+) value=(\d+)$', re.M)
Q1_STARTS = ((0, 4), (4, 4), (4, 0))
SCENARIO = re.compile(r'SCENARIO seed=(\d+) entrance=(\d+) start=\((\d+),(\d+)\) yaw=[-\d.]+ map=Q([1-4]) end=\((\d+),(\d+)\)')
FINAL = re.compile(r'^(PASS|FAIL): (.*?) t=([\d.]+) pose=\(([-\d.]+),([-\d.]+)\) state=(\w+) seed=(\d+)$', re.M)
METRICS = re.compile(r'^METRICS (.*)$', re.M)


def utc():
    return datetime.now(timezone.utc).isoformat()


def rotate(cell, turns):
    x, y = cell
    for _ in range(turns):
        x, y = 4 - y, x
    return (x, y)


def run_one(exe, case, mode, seed, timeout):
    profile, variant, entrance, heading, trial = case
    mask = 15 if profile == 'blind' else 1 << variant
    command = [str(exe), '--headless', '--quiet', '--map', 'q1', '--surface', 'whiteboard',
               '--variant', str(variant), '--entrance', str(entrance), '--heading', str(heading),
               '--variant-mask', str(mask)]
    if seed is not None:
        command += ['--seed', str(seed)]
    if mode == 'rhr':
        command += ['--force-rhr']
    start = time.perf_counter()
    try:
        child = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               timeout=timeout)
        raw, code = child.stdout, child.returncode
    except subprocess.TimeoutExpired as error:
        raw = (error.stdout or b'') + b'\nFAIL: host wall watchdog\n'
        code = 124
    elapsed = time.perf_counter() - start
    output = raw.decode(errors='replace')
    seed_record = SEED.search(output)
    if seed is None:
        if not seed_record or seed_record[1] != 'rdseed':
            raise RuntimeError('RDSEED draw missing; campaign stopped without replacing the failed draw: ' + output[-1000:])
        seed = int(seed_record[2])
    errors = []
    if not seed_record or int(seed_record[2]) != seed:
        errors.append('seed metadata mismatch')
    if mode == 'rhr' and seed_record and seed_record[1] != 'explicit':
        errors.append('paired replay seed missing')
    scenario = SCENARIO.search(output)
    if not scenario:
        errors.append('missing scenario')
    else:
        s, e, x, y, v, gx, gy = map(int, scenario.groups())
        if (s, e, v) != (seed, entrance, variant + 1):
            errors.append('scenario mismatch')
        if (x, y) != rotate(Q1_STARTS[entrance], variant):
            errors.append('start mismatch')
        if (gx, gy) != rotate((0, 0), variant):
            errors.append('fixed goal mismatch')
    if f'draw entrance=override heading=override cardinal={(heading + variant) % 4} end=fixed' not in output:
        errors.append('heading or fixed-goal mismatch')
    if f'arena=supplied variant_mask={mask} ' not in output or 'SURFACE model=whiteboard' not in output:
        errors.append('map mask or surface mismatch')
    final = FINAL.search(output)
    metric_match = METRICS.search(output)
    metrics = dict(re.findall(r'(\w+)=([-\w.]+)', metric_match[1])) if metric_match else {}
    if not final:
        errors.append('missing final result')
    if not metrics:
        errors.append('missing metrics')
    if metrics and metrics.get('forced_rhr') != str(int(mode == 'rhr')):
        errors.append('engine engagement mismatch')
    if metrics and metrics.get('run_budget_us') != '86000000':
        errors.append('run budget mismatch')
    passed = bool(code == 0 and final and final[1] == 'PASS' and final[6] == 'GOAL')
    if passed:
        if any(metrics.get(k) != '1' for k in ('tape', 'zero_pwm', 'stopped', 'motor_goal')) or metrics.get('collision') != '0':
            errors.append('physical goal/safe-stop evidence mismatch')
        if float(metrics.get('end_error_m', 'inf')) > .025001 or float(metrics.get('goal_from_key_s', '-1')) < 0:
            errors.append('goal distance or timing mismatch')
    state = final[6] if final else 'UNKNOWN'
    if passed:
        outcome = 'GOAL'
    elif metrics.get('collision') == '1' or 'physical wall collision' in output:
        outcome = 'COLLISION'
    elif code == 124:
        outcome = 'HOST_WATCHDOG'
    elif state == 'TIMEOUT':
        outcome = 'TIMEOUT'
    elif 'lifecycle watchdog' in output:
        outcome = 'STARTUP_WATCHDOG'
    else:
        outcome = 'PROCESS_ERROR' if code not in (0, 1) else 'FAILURE'
    if final and int(final[7]) != seed:
        errors.append('final seed mismatch')
    record = dict(knowledge=profile, variant=variant, entrance=entrance, heading_q1=heading,
                  heading_world=(heading + variant) % 4, trial=trial, mode=mode, seed=seed,
                  seed_origin='rdseed' if mode == 'feature' else 'paired-rdseed', variant_mask=mask,
                  passed=passed and not errors, outcome=outcome, state=state, returncode=code,
                  goal_from_key_s=float(metrics['goal_from_key_s']) if passed and metrics else None,
                  finish_boot_s=float(final[3]) if final else None,
                  end_error_m=float(metrics['end_error_m']) if metrics else None,
                  metrics=metrics, reason=final[2] if final else output[-200:],
                  validation_errors=errors, host_wall_s=round(elapsed, 6), command=command,
                  stdout_sha256=hashlib.sha256(raw).hexdigest(), stdout_bytes=len(raw),
                  faults=re.findall(r'(?:PATH|RHR) (?:FAULT|BLOCKED|NO_PATH) [^\n]+', output))
    return record, raw


def wilson(passed, n):
    z = 1.959963984540054
    p = passed / n
    den = 1 + z*z/n
    middle = (p + z*z/(2*n)) / den
    half = z * math.sqrt(p*(1-p)/n + z*z/(4*n*n)) / den
    return max(0, middle-half), min(1, middle+half)


def percentile(values, fraction):
    if not values:
        return None
    x = sorted(values)
    position = (len(x)-1)*fraction
    lo = int(position)
    hi = min(lo+1, len(x)-1)
    return x[lo] + (x[hi]-x[lo])*(position-lo)


def summarize(rows):
    n = len(rows)
    passed = sum(r['passed'] for r in rows)
    low, high = wilson(passed, n)
    times = [r['goal_from_key_s'] for r in rows if r['passed']]
    return dict(n=n, passed=passed, reliability_index=passed/n, wilson_95_low=low,
                wilson_95_high=high, outcomes=dict(Counter(r['outcome'] for r in rows)),
                goal_time_median_s=percentile(times, .5), goal_time_p95_s=percentile(times, .95),
                goal_time_min_s=min(times) if times else None, goal_time_max_s=max(times) if times else None,
                validation_errors=sum(bool(r['validation_errors']) for r in rows))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output-dir', required=True, type=Path)
    parser.add_argument('--samples', type=int, default=512)
    parser.add_argument('--jobs', type=int, default=12)
    parser.add_argument('--wall-timeout', type=float, default=45)
    args = parser.parse_args()
    if args.samples < 1 or args.jobs < 1:
        parser.error('samples and jobs must be positive')
    exe = args.executable.resolve()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    local_key = hashlib.sha256(str(out).encode()).hexdigest()[:16]
    db = sqlite3.connect(Path(tempfile.gettempdir()) / f'cprobot-reliability-{local_key}.sqlite3')
    db.execute('CREATE TABLE IF NOT EXISTS meta (key TEXT PRIMARY KEY, value TEXT)')
    db.execute('CREATE TABLE IF NOT EXISTS runs (id TEXT PRIMARY KEY, record TEXT, stdout_zlib BLOB)')
    signature = dict(executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(), samples=args.samples,
                     axes='4 rotations x 3 entrances x 4 headings x 2 search profiles x 2 modes',
                     q1_start_cells=[list(c) for c in Q1_STARTS],
                     jobs=args.jobs, wall_timeout_s=args.wall_timeout)
    durable_meta = out/'checkpoint-meta.json'
    saved_meta = json.loads(durable_meta.read_text()) if durable_meta.exists() else None
    if saved_meta and saved_meta['signature'] != signature:
        parser.error('persistent checkpoint signature differs; use a new output directory')
    previous = db.execute("SELECT value FROM meta WHERE key='signature'").fetchone()
    if previous and json.loads(previous[0]) != signature:
        parser.error('checkpoint signature differs; use a new output directory')
    if not previous:
        db.execute('INSERT INTO meta VALUES (?,?)', ('signature', json.dumps(signature)))
        db.execute('INSERT INTO meta VALUES (?,?)', ('started_utc', utc()))
        db.commit()
    started_utc = db.execute("SELECT value FROM meta WHERE key='started_utc'").fetchone()[0]
    if saved_meta:
        started_utc = saved_meta['started_utc']
    else:
        temporary = durable_meta.with_suffix('.tmp')
        temporary.write_text(json.dumps(dict(signature=signature, started_utc=started_utc))+'\n')
        temporary.replace(durable_meta)
    checkpoint_dir = out/'checkpoint-parts'
    checkpoint_dir.mkdir(exist_ok=True)
    parts = sorted(checkpoint_dir.glob('*.jsonl.gz'))
    def pair_id(case):
        return ':'.join(map(str, case))
    for part in parts:
        with gzip.open(part, 'rt', encoding='utf-8') as stream:
            for text in stream:
                saved = json.loads(text)
                raw = saved.pop('stdout').encode()
                assert hashlib.sha256(raw).hexdigest() == saved['stdout_sha256']
                case = tuple(saved[k] for k in ('knowledge','variant','entrance','heading_q1','trial'))
                db.execute('INSERT OR IGNORE INTO runs VALUES (?,?,?)',
                           (pair_id(case)+':'+saved['mode'], json.dumps(saved), zlib.compress(raw, 6)))
    db.commit()
    completed = Counter(r[0].rsplit(':', 1)[0] for r in db.execute('SELECT id FROM runs'))
    assert all(v == 2 for v in completed.values()), 'partial pair checkpoint'
    buffered = []
    part_number = [len(parts)]
    def persist_pending():
        if not buffered:
            return
        payload = ''.join(json.dumps({**record, 'stdout': raw.decode()})+'\n'
                          for record, raw in buffered).encode()
        destination = checkpoint_dir/f'part-{part_number[0]:06d}.jsonl.gz'
        temporary = destination.with_suffix('.tmp')
        temporary.write_bytes(gzip.compress(payload, compresslevel=1, mtime=0))
        temporary.replace(destination)
        db.commit()
        buffered.clear()
        part_number[0] += 1
    # Interleave permutations at each trial to distribute host conditions.
    plan = [(p,v,e,h,i) for i in range(args.samples)
            for p,v,e,h in itertools.product(('configured','blind'), range(4), range(len(Q1_STARTS)), range(4))]
    todo = [c for c in plan if pair_id(c) not in completed]
    def run_pair(case):
        feature = run_one(exe, case, 'feature', None, args.wall_timeout)
        rhr = run_one(exe, case, 'rhr', feature[0]['seed'], args.wall_timeout)
        return case, (feature, rhr)
    wall_start = time.perf_counter()
    done = len(completed)
    report_at = wall_start
    print(f'START {len(plan)} pairs / {2*len(plan)} missions; {len(todo)} pairs pending; jobs={args.jobs}', flush=True)
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        iterator = iter(todo)
        pending = {pool.submit(run_pair, case) for case in itertools.islice(iterator, args.jobs*2)}
        while pending:
            finished, pending = wait(pending, return_when=FIRST_COMPLETED)
            for future in finished:
                case, pair = future.result()
                for record, raw in pair:
                    db.execute('INSERT INTO runs VALUES (?,?,?)',
                               (pair_id(case)+':'+record['mode'], json.dumps(record), zlib.compress(raw, 6)))
                buffered.extend(pair)
                done += 1
                if len(buffered) >= 128 or done == len(plan):
                    persist_pending()
                case = next(iterator, None)
                if case is not None:
                    pending.add(pool.submit(run_pair, case))
            now = time.perf_counter()
            if now-report_at >= 15 or done == len(plan):
                persist_pending()
                seconds = now-wall_start
                print(f'PROGRESS {done}/{len(plan)} pairs ({2*done} missions); wall={seconds:.1f}s; '
                      f'rate={2*(done-len(completed))/max(seconds,.001):.2f} missions/s', flush=True)
                (out/'progress.json').write_text(json.dumps(dict(pairs_done=done, pairs_total=len(plan), updated_utc=utc(), segment_wall_s=seconds))+'\n')
                report_at = now
    rows = []
    with gzip.open(out/'runs.jsonl.gz', 'wt', encoding='utf-8', compresslevel=6) as stream:
        for _, text, packed in db.execute('SELECT * FROM runs ORDER BY id'):
            record = json.loads(text)
            raw = zlib.decompress(packed)
            assert hashlib.sha256(raw).hexdigest() == record['stdout_sha256']
            stream.write(json.dumps({**record, 'stdout': raw.decode(errors='replace')})+'\n')
            rows.append(record)
    assert len(rows) == 2*len(plan)
    groups = defaultdict(list)
    pairs = defaultdict(dict)
    for r in rows:
        groups[(r['mode'],r['knowledge'],r['variant'],r['entrance'],r['heading_q1'])].append(r)
        pairs[(r['knowledge'],r['variant'],r['entrance'],r['heading_q1'],r['trial'])][r['mode']] = r
    assert len(groups)==2 * 2 * 4 * len(Q1_STARTS) * 4 and all(len(g)==args.samples for g in groups.values())
    assert all(set(r['trial'] for r in g)==set(range(args.samples)) for g in groups.values())
    assert all(p['feature']['seed']==p['rhr']['seed'] for p in pairs.values())
    per_case=[dict(mode=k[0],knowledge=k[1],variant=k[2],entrance=k[3],heading_q1=k[4],
                   **summarize(g)) for k,g in sorted(groups.items())]
    summary=dict(**signature, started_utc=started_utc, finished_utc=utc(), total=len(rows), pairs=len(pairs),
                 modes={m:summarize([r for r in rows if r['mode']==m]) for m in ('feature','rhr')},
                 permutations=per_case, paired_outcomes=dict(Counter(
                     ('both_pass' if p['feature']['passed'] and p['rhr']['passed'] else
                      'feature_only' if p['feature']['passed'] else
                      'rhr_only' if p['rhr']['passed'] else 'neither') for p in pairs.values())),
                 distinct_rdseed_draws=len({p['feature']['seed'] for p in pairs.values()}),
                 archive_records_verified=len(rows), complete=True)
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    columns=['knowledge','variant','entrance','heading_q1','heading_world','trial','mode','seed',
             'seed_origin','variant_mask','passed','outcome','state','returncode','goal_from_key_s',
             'finish_boot_s','end_error_m','host_wall_s','stdout_sha256','stdout_bytes','reason']
    with gzip.open(out/'results.csv.gz','wt',newline='',encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=columns,extrasaction='ignore');w.writeheader();w.writerows(rows)
    db.close()
    print('COMPLETE',json.dumps(summary['modes']),flush=True)
    return 0 if not any(r['validation_errors'] for r in rows) else 2


if __name__=='__main__':
    raise SystemExit(main())

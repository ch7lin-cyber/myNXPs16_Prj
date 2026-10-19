#!/usr/bin/env python3
"""Offline CH0 trace checks; standard-library only. Never claims MCU provenance."""
import argparse
import csv
import math
from collections import deque

FIELDS = 'init_ms now_ms elapsed_ms lateness_ms sequence pv provider_ok input_valid accepted sample_count output_valid average status cycles cycles_valid'.split()


def analyze(rows, cpu_hz=None, max_us=None, max_lateness_ms=None):
    window = deque(maxlen=5)
    last_seq = None
    last_time = release = None
    failures = []
    coverage = set()
    invalid_seen = False
    times, lateness, jitter = [], [], []
    for n, r in enumerate(rows, 1):
        def check(ok, message):
            if not ok:
                failures.append(f'row {n}: {message}')
        if last_time is None:
            last_time = release = r['init_ms']
        check(r['init_ms'] == rows[0]['init_ms'], 'capture mixes initializations')
        dt = (r['now_ms'] - last_time) & 0xffffffff
        age = (r['now_ms'] - release) & 0xffffffff
        check(age >= 20 and age < 0x80000000, 'callback before due or invalid clock')
        check(r['elapsed_ms'] == dt, 'elapsed_ms mismatch or missing rows')
        check(r['lateness_ms'] == age - 20, 'release lateness mismatch')
        release = (r['now_ms'] - age % 20) & 0xffffffff
        last_time = r['now_ms']
        check(r['status'] == 0, 'FB status is not OK')
        valid = r['provider_ok'] and r['input_valid']
        accepted = bool(valid and (last_seq is None or r['sequence'] != last_seq))
        check(bool(r['accepted']) == accepted, 'sample_sequence gating mismatch')
        if not valid:
            window.clear()
            last_seq = None
            invalid_seen = True
            coverage.add('invalid_clear')
        elif accepted:
            if invalid_seen:
                coverage.add('recovery')
                invalid_seen = False
            window.append(r['pv'])
            last_seq = r['sequence']
            coverage.add('valid')
            if len(window) == 5:
                coverage.add('five_samples')
        else:
            coverage.add('duplicate')
        expected = sum(window) / len(window) if window else 0.0
        check(r['sample_count'] == len(window), 'sample_count mismatch')
        check(bool(r['output_valid']) == bool(window), 'output_valid mismatch')
        check(math.isfinite(r['average']) and math.isclose(r['average'], expected, rel_tol=0, abs_tol=1e-6),
              f'average {r["average"]} != {expected}')
        lateness.append(r['lateness_ms'])
        jitter.append(abs(dt - 20))
        if r['cycles_valid'] and cpu_hz:
            times.append(r['cycles'] * 1e6 / cpu_hz)
    required = {'valid', 'five_samples', 'duplicate', 'invalid_clear', 'recovery'}
    missing = sorted(required - coverage)
    functional_failures = list(failures)
    if max_lateness_ms is not None and lateness and max(lateness) > max_lateness_ms:
        failures.append('release lateness exceeds supplied budget')
    if max_us is not None and times and max(times) > max_us:
        failures.append('execution time exceeds supplied budget')
    timing_complete = bool(rows and len(times) == len(rows) and max_us is not None and max_lateness_ms is not None)
    return dict(functional='FAIL' if functional_failures else ('INCOMPLETE' if missing else 'PASS'),
                timing='INCOMPLETE' if not timing_complete else ('FAIL' if failures else 'PASS'),
                missing=missing, failures=failures,
                max_lateness_ms=max(lateness, default=0), max_abs_interval_jitter_ms=max(jitter, default=0),
                max_adapter_us=max(times, default=None), rows=len(rows))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('csv')
    p.add_argument('--cpu-hz', type=float)
    p.add_argument('--max-us', type=float)
    p.add_argument('--max-lateness-ms', type=float)
    a = p.parse_args()
    for value in (a.cpu_hz, a.max_us, a.max_lateness_ms):
        if value is not None and (not math.isfinite(value) or value <= 0):
            p.error('timing parameters must be finite and positive')
    try:
        with open(a.csv, newline='') as f:
            reader = csv.DictReader(f)
            if not set(FIELDS).issubset(reader.fieldnames or []):
                raise ValueError('missing CSV fields')
            rows = [{k: float(r[k]) if k == 'average' else int(r[k]) for k in FIELDS} for r in reader]
        if not rows:
            raise ValueError('empty capture')
        report = analyze(rows, a.cpu_hz, a.max_us, a.max_lateness_ms)
    except (OSError, ValueError, KeyError) as exc:
        p.error(str(exc))
    import json
    print(json.dumps(report, indent=2))
    return 1 if report['failures'] else (2 if report['functional'] != 'PASS' or report['timing'] != 'PASS' else 0)


if __name__ == '__main__':
    raise SystemExit(main())

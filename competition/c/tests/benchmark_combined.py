#!/usr/bin/env python3
"""Frozen four-way phase/factoring interaction test on a hash-verified manifest."""
import argparse
import json
import math
from pathlib import Path
import shlex
import subprocess
import sys
from benchmark import digest, verified_manifest


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--solver', required=True, type=Path)
    p.add_argument('--manifest', required=True, type=Path)
    p.add_argument('--checker', required=True)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--seconds', type=float, default=10)
    p.add_argument('--repeats', type=int, default=2)
    a = p.parse_args()
    if not math.isfinite(a.seconds) or a.seconds <= 0 or a.repeats < 1:
        p.error('seconds must be finite and positive; repeats must be positive')
    inputs, _ = verified_manifest(a.manifest)
    base = [str(a.solver.resolve()), '--chrono', '--congruence', '--equiv',
            '--equiv-budget', '100000000', '--alternating']
    factor = ['--factor', '--factor-min-gain', '32']
    profiles = {'control': [], 'no-rephase': ['--no-rephase'], 'factor': factor,
                'both': ['--no-rephase', *factor]}
    commands = {k: base + v + ['--binary-proof', '--proof', '{proof}', '{input}']
                for k, v in profiles.items()}
    policy = dict(scope=__doc__, inputs=inputs, commands=commands,
                  solver_sha256=digest(a.solver), wall_seconds=a.seconds,
                  repeats=a.repeats, seed=2026090831,
                  promotion='No correctness failures; no lost verified inputs; >=10% aggregate PAR2 improvement. A short screen alone cannot promote defaults.')
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.with_suffix('.policy.json').write_text(json.dumps(policy, indent=2) + '\n')
    cmd = [sys.executable, str(Path(__file__).with_name('benchmark.py')),
           '--external-wall-only', '--split', 'heldout', '--manifest', str(a.manifest),
           '--checker', a.checker, '--check-timeout', '600', '--timeout', str(a.seconds),
           '--repeats', str(a.repeats), '--seed', str(policy['seed']), '--output', str(a.output)]
    for k, v in commands.items():
        cmd += ['--solver', k + '=' + shlex.join(v)]
    subprocess.run(cmd, check=True)


if __name__ == '__main__':
    main()

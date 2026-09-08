#!/usr/bin/env python3
"""Frozen serial search ablations, with optional intrusive phase accounting."""
import argparse
import json
import math
import os
from pathlib import Path
import subprocess
import sys
from benchmark import digest


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--solver', type=Path, required=True)
    p.add_argument('--checker', required=True)
    p.add_argument('--seconds', type=float, default=10)
    p.add_argument('--repeats', type=int, default=1)
    p.add_argument('--accounting', action='store_true')
    p.add_argument('--suite', choices=['basic', 'maintenance'], default='basic')
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('inputs', nargs='+', type=Path)
    a = p.parse_args()
    if not math.isfinite(a.seconds) or a.seconds <= 0 or a.repeats < 1:
        p.error('seconds must be finite and positive; repeats must be positive')
    base = ['--chrono', '--congruence', '--equiv', '--equiv-budget', '100000000', '--alternating']
    profiles = {'control': base, 'vmtf': base + ['--vmtf'],
                'no-probing': base + ['--no-probing'],
                'no-chrono': [x for x in base if x != '--chrono']}
    if a.suite == 'maintenance':
        profiles = {'control': base, 'growing-reduction': base + ['--reduce-increment', '1000'],
                    'no-rephase': base + ['--no-rephase'],
                    'walk': base + ['--local-search', '--ls-save-phases']}
    suffix = ['--time', str(a.seconds), '--binary-proof', '--proof', '{proof}', '{input}']
    if a.accounting:
        suffix = ['--accounting'] + suffix
    import shlex
    commands = {k: [str(a.solver.resolve()), *v, *suffix] for k, v in profiles.items()}
    policy = {'scope': __doc__, 'accounting': a.accounting,
              'solver_sha256': digest(a.solver), 'commands': commands,
              'inputs': {str(x.resolve()): {'sha256': digest(x)} for x in a.inputs},
              'cpu_seconds': a.seconds, 'outer_wall_seconds': a.seconds + 5,
              'repeats': a.repeats, 'seed': 2026090901}
    a.output.parent.mkdir(parents=True, exist_ok=True)
    manifest = a.output.with_suffix('.policy.json')
    manifest.write_text(json.dumps(policy, indent=2) + '\n')
    cmd = [sys.executable, str(Path(__file__).with_name('benchmark.py')),
           '--checker', a.checker, '--check-timeout', '600', '--timeout', str(a.seconds + 5),
           '--repeats', str(a.repeats), '--seed', str(policy['seed']),
           '--manifest', str(manifest), '--output', str(a.output)]
    for name, command in commands.items():
        cmd += ['--solver', name + '=' + shlex.join(command)]
    subprocess.run(cmd, check=True, env=os.environ)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Exercise short CPU deadlines through search, minimization and preprocessing.

This checks resource-limit behavior, not general answer correctness. Pigeonhole
inputs are known UNSAT; the empty formula is SAT. Certificate validation lives
in validate.py. Parsing/startup is outside the solver's CPU-limit interval.
"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--solver', required=True, type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    solver = str(args.solver.resolve())
    records = []
    with tempfile.TemporaryDirectory(prefix='bsat-deadlines-') as directory:
        root = Path(directory)
        holes = 9
        def var(p, h): return p * holes + h + 1
        clauses = [[var(p, h) for h in range(holes)] for p in range(holes+1)]
        clauses += [[-var(p, h), -var(q, h)] for h in range(holes)
                    for p in range(holes+1) for q in range(p+1, holes+1)]
        hard = root/'pigeonhole.cnf'
        hard.write_text(f'p cnf {holes*(holes+1)} {len(clauses)}\n' +
                        ''.join(' '.join(map(str, c))+' 0\n' for c in clauses))
        empty = root/'empty.cnf';empty.write_text('p cnf 100000 0\n')
        modes = [(hard, ['--local-search', '--ls-interval', '1', '--no-probing']),
                 (hard, ['--local-search', '--ls-interval', '1', '--no-probing', '--vmtf', '--reuse-trail']),
                 (hard, ['--reuse-trail']),
                 (hard, ['--reuse-trail', '--vmtf']),
                 (hard, []), (hard, ['--iterative-minimize']),
                 (hard, ['--equiv', '--elim', '--bce', '--dynamic-lbd', '--binary-proof']),
                 (empty, ['--no-probing']),
                 (hard, ['--binary-minimize', '--vmtf']),
                 (hard, ['--random-phase', '--vmtf']),
                 (hard, ['--vmtf']),
                 (empty, ['--vmtf', '--no-probing'])]
        modes += [(inp, options + ['--ls-save-phases'])
                  for inp, options in modes if '--local-search' in options]
        modes += [(hard, ['--portfolio', '0.000000000001']),
                  (hard, ['--portfolio', '0.002', '--binary-proof']),
                  (empty, ['--portfolio', '0.000000000001', '--no-probing'])]
        modes += [(hard, ['--congruence', '--equiv', '--binary-proof']),
                  (empty, ['--congruence', '--no-probing'])]
        for limit in (0.001, 0.01, 0.05):
            for inp, options in modes:
                cmd = [solver, '--time', str(limit), '--proof', str(root/'proof.drat'), *options, str(inp)]
                start = time.monotonic()
                run = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
                wall = time.monotonic() - start
                status = re.search(r'^s (SATISFIABLE|UNSATISFIABLE|UNKNOWN)$', run.stdout, re.M)
                cpu = re.search(r'^c CPU time\s*:\s*([0-9.]+) s$', run.stdout, re.M)
                assert status and cpu, (cmd, run.returncode, run.stdout, run.stderr)
                expected = 'SATISFIABLE' if inp == empty else 'UNSATISFIABLE'
                assert status[1] in ('UNKNOWN', expected), (cmd, status[1])
                assert run.returncode == {'UNKNOWN': 0, 'SATISFIABLE': 10, 'UNSATISFIABLE': 20}[status[1]]
                elapsed = float(cpu[1])
                # Generous CI margin: detect a lost poll without timing a few microseconds.
                assert elapsed <= limit + 0.1, (cmd, elapsed, limit)
                records.append(dict(input=inp.name, options=options, limit=limit,
                                    solving_cpu=elapsed, wall_seconds=wall, status=status[1]))
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(dict(solver=solver, runs=records), indent=2)+'\n')
    print(f'PASS: {len(records)} short-deadline runs; maximum observed CPU overrun '
          f'{max(r["solving_cpu"]-r["limit"] for r in records):.3f}s')


if __name__ == '__main__':
    main()

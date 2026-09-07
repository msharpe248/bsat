#!/usr/bin/env python3
"""Reproducible focused validation of local-search option combinations."""
import argparse
import random
import tempfile
from pathlib import Path
from validate import run_case


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--solver', required=True, type=Path)
    parser.add_argument('--checker', required=True, type=Path)
    args = parser.parse_args()
    solver, checker = str(args.solver.resolve()), str(args.checker.resolve())
    configs = [
        ['--local-search', '--ls-interval', '1', '--no-probing'],
        ['--local-search', '--ls-interval', '1', '--no-probing', '--vmtf'],
        ['--local-search', '--ls-interval', '1', '--no-probing', '--vmtf',
         '--reuse-trail', '--binary-proof'],
        ['--local-search', '--ls-interval', '1', '--random-phase', '--inprocess',
         '--inprocess-interval', '1'],
        ['--local-search', '--ls-interval', '1', '--equiv', '--iterative-minimize'],
        ['--local-search', '--ls-interval', '1', '--no-phase-saving', '--binary-proof'],
    ]
    formulas = [(2, [[1, 2], [1, -2], [-1, 2]]),
                (2, [[1, 2], [1, -2], [-1, 2], [-1, -2]])]
    rng = random.Random(20261004)
    for _ in range(50):
        n = rng.randint(2, 9)
        clauses = [[v if rng.randrange(2) else -v
                    for v in rng.sample(range(1, n+1), rng.randint(1, min(n, 4)))]
                   for _ in range(rng.randint(n, 6*n))]
        formulas.append((n, clauses))
    count = 0
    with tempfile.TemporaryDirectory(prefix='bsat-transfer-validation-') as tmp:
        for n, clauses in formulas:
            for options in configs:
                error = run_case(solver, n, clauses, options, Path(tmp), checker, None)
                if error:
                    raise RuntimeError((options, n, clauses, error))
                count += 1
    print(f'PASS: {count} local-search model/proof checks; seed 20261004')


if __name__ == '__main__':
    main()

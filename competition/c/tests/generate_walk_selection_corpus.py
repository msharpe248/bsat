#!/usr/bin/env python3
"""Generate fixed-work UNSAT walks; UNKNOWN is expected, not an UNSAT proof."""
import argparse
import hashlib
import json
import random
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=True)
    inputs = {}
    for n in (1000, 10000):
        seed = 20261006 + n
        rng = random.Random(seed)
        clauses = [[1, 2], [1, -2], [-1, 2], [-1, -2]]
        for _ in range(21*n//5):
            clauses.append([v if rng.getrandbits(1) else -v
                            for v in rng.sample(range(1, n+1), 3)])
        path = root/f'vars{n}.cnf'
        path.write_text(f'c fixed-work walk development seed {seed}\np cnf {n} {len(clauses)}\n' +
                        ''.join(' '.join(map(str, c))+' 0\n' for c in clauses))
        inputs[path.name] = {'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                             'generator_seed': seed}
    (root/'manifest.json').write_text(json.dumps({
        'schema': 1, 'policy': 'random 3-SAT plus contradictory binary core; fixed flip work',
        'inputs': inputs}, indent=2)+'\n')


if __name__ == '__main__':
    main()

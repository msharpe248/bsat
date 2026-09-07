#!/usr/bin/env python3
"""Generate the fixed, planted 3-SAT development corpus for walk-score evaluation."""
import hashlib
import json
import random
from pathlib import Path

root = Path(__file__).resolve().parent.parent
inputs = {}
for n in (50, 100, 200):
    for index in range(2):
        seed = 20261005 + 10*n + index
        rng = random.Random(seed)
        model = [False] + [bool(rng.getrandbits(1)) for _ in range(n)]
        clauses = []
        for _ in range(21*n//5):
            while True:
                clause = [v if rng.getrandbits(1) else -v
                          for v in rng.sample(range(1, n+1), 3)]
                if any(model[abs(lit)] == (lit > 0) for lit in clause):
                    clauses.append(clause)
                    break
        path = root/'tests'/'fixtures'/'local_search_scores'/f'vars{n}'/f'seed{index}.cnf'
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(f'c planted 3-SAT development seed {seed}\np cnf {n} {len(clauses)}\n' +
                        ''.join(' '.join(map(str, clause))+' 0\n' for clause in clauses))
        key = '../' + str(path.relative_to(root))
        inputs[key] = {'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                       'generator_seed': seed,
                       'planted_model': [v if model[v] else -v for v in range(1, n+1)]}
(root/'benchmark_results'/'walk-score-corpus-20260907.json').write_text(
    json.dumps({'schema': 1, 'policy': 'fixed planted 3-SAT development inputs, 4.2 clauses/variable',
                'inputs': inputs}, indent=2)+'\n')

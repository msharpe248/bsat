#!/usr/bin/env python3
"""Reproduce the fixed development stress sample used for dynamic-LBD testing."""
import hashlib
import json
from pathlib import Path
import random
import sys

root = Path(sys.argv[1])
if root.exists() and any(root.iterdir()):
    raise SystemExit('output directory must be empty')
root.mkdir(parents=True, exist_ok=True)
seed = int(sys.argv[2]) if len(sys.argv) > 2 else 20260911
rng = random.Random(seed)
entries = []
def save(family, name, n, clauses):
    p = root / family / (name + '.cnf')
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(f'p cnf {n} {len(clauses)}\n' + ''.join(' '.join(map(str, c)) + ' 0\n' for c in clauses))
    entries.append(dict(path=str(p.resolve()), family=family, variables=n, clauses=len(clauses), sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
for n in (180, 220, 260):
    for i in range(4):
        clauses = [[v if rng.randrange(2) else -v for v in rng.sample(range(1,n+1),3)] for _ in range(round(4.26*n))]
        save('random3sat', f'v{n}-{i}', n, clauses)
for holes in (7, 8, 9):
    def v(p,h): return p*holes+h+1
    clauses = [[v(p,h) for h in range(holes)] for p in range(holes+1)]
    clauses += [[-v(p,h),-v(q,h)] for h in range(holes) for p in range(holes+1) for q in range(p+1,holes+1)]
    save('pigeonhole', f'php-{holes+1}-{holes}', holes*(holes+1), clauses)
(root/'manifest.json').write_text(json.dumps(dict(seed=seed,split='development',instances=entries),indent=2)+'\n')

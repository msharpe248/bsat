#!/usr/bin/env python3
"""Deterministic long, overlapping resolution pairs for elim_driver."""
import hashlib
import json
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
root.mkdir(parents=True, exist_ok=True)
inputs = {}
for n in (1000, 5000, 10000):
    path = root / f'overlap-{n}.cnf'
    tail = ' '.join(map(str, range(2, n + 1)))
    path.write_text(f'p cnf {n} 2\n1 {tail} 0\n-1 {tail} 0\n')
    inputs[str(path)] = {'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                         'family': f'overlap-{n}'}
(root / 'manifest.json').write_text(json.dumps({'schema': 1, 'inputs': inputs}, indent=2) + '\n')

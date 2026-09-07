#!/usr/bin/env python3
"""Deterministic pure-clause reconstruction-staging inputs for elim_driver."""
import hashlib
import json
import sys
from pathlib import Path

root = Path(sys.argv[1]).resolve()
root.mkdir(parents=True, exist_ok=True)
inputs = {}
for n in (10000, 50000, 100000):
    path = root / f'pure-{n}.cnf'
    path.write_text(f'p cnf {n} 1\n' + ' '.join(map(str, range(1, n + 1))) + ' 0\n')
    inputs[str(path)] = {'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                         'family': f'pure-{n}'}
(root / 'manifest.json').write_text(json.dumps({'schema': 1, 'inputs': inputs}, indent=2) + '\n')

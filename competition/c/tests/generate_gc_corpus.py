#!/usr/bin/env python3
"""Generate positive clause databases for isolated collection measurements."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    inputs = {}
    clause = ' '.join(map(str, range(1, 33))) + ' 0\n'
    for count in (10000, 50000, 100000):
        path = (args.output / f'clauses-{count}.cnf').resolve()
        data = (f'p cnf 32 {count}\n' + clause * count).encode()
        path.write_bytes(data)
        inputs[str(path)] = {'sha256': hashlib.sha256(data).hexdigest()}
    (args.output / 'manifest.json').write_text(json.dumps({'schema': 1, 'policy': 'Positive 32-literal clauses; collection storage diagnostic, not search difficulty', 'inputs': inputs}, indent=2) + '\n')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Generate declared-variable storage cases with trivial UNSAT certificates."""
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
    for size in (100000, 500000, 1000000):
        path = (args.output / f'vars-{size}.cnf').resolve()
        data = f'p cnf {size} 1\n0\n'.encode()
        path.write_bytes(data)
        inputs[str(path)] = {'sha256': hashlib.sha256(data).hexdigest()}
    manifest = {'schema': 1, 'policy': 'Declared-variable allocation with an original empty clause; storage diagnostic, not search difficulty', 'inputs': inputs}
    (args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Generate reducible and irreducible clause databases for vivification diagnostics."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--irreducible', action='store_true', help='Generate clauses with no removable literal')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    inputs = {}
    for size in (8, 32, 64):
        clauses = [] if args.irreducible else [[1, 2, 3], [1, 2, -3]]
        for i in range(100):
            first = 4 + i * (size - 2)
            clauses.append([1, 2, *range(first, first + size - 2)])
        variables = max(abs(lit) for clause in clauses for lit in clause)
        data = (f'p cnf {variables} {len(clauses)}\n' + ''.join(
            ' '.join(map(str, clause)) + ' 0\n' for clause in clauses)).encode()
        name = 'irreducible' if args.irreducible else 'prefix'
        path = (args.output / f'{name}-{size}.cnf').resolve()
        path.write_bytes(data)
        inputs[str(path)] = {'sha256': hashlib.sha256(data).hexdigest()}
    (args.output / 'manifest.json').write_text(json.dumps({
        'schema': 1, 'policy': ('100 irreducible positive clauses' if args.irreducible else
                                '100 redundant clauses with two-literal RUP core') +
                               '; vivification kernel diagnostic, not search difficulty',
        'inputs': inputs}, indent=2) + '\n')


if __name__ == '__main__':
    main()

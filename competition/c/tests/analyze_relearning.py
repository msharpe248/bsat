#!/usr/bin/env python3
"""Count exact repeated clause additions in an exported retained binary journal."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--report', type=Path, required=True)
    p.add_argument('--journal', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    report = json.loads(a.report.read_text())
    assert report['complete'] and report['runs'][-1]['result'] == 10
    data = a.journal.read_bytes()
    assert len(data) == report['runs'][-1]['journal_bytes']
    rows = [dict(depth=r['depth'], polarity=r['polarity'], additions=0, unique=0,
                 repeated_within_query=0, repeated_from_prior_query=0,
                 repeated_literals=0, added_literals=0) for r in report['runs']]
    seen = {}
    query = pos = 0
    while pos < len(data):
        while pos >= report['runs'][query]['journal_bytes']:
            query += 1
        assert data[pos] == ord('a'), (pos, data[pos])
        pos += 1
        clause = []
        while data[pos]:
            value = shift = 0
            while True:
                byte = data[pos]
                pos += 1
                value |= (byte & 127) << shift
                if byte < 128:
                    break
                shift += 7
                assert shift < 35
            assert value >= 2
            clause.append(value)
        pos += 1
        assert pos <= report['runs'][query]['journal_bytes']
        assert clause
        clause.sort()
        key = struct.pack('<' + 'I' * len(clause), *clause)
        row = rows[query]
        row['additions'] += 1
        row['added_literals'] += len(clause)
        if key in seen:
            last, count = seen[key]
            row['repeated_within_query' if last == query else 'repeated_from_prior_query'] += 1
            row['repeated_literals'] += len(clause)
            seen[key] = query, count + 1
        else:
            row['unique'] += 1
            seen[key] = query, 1
    for row in rows:
        row['repeated_fraction'] = (row['repeated_within_query'] + row['repeated_from_prior_query']) / max(1, row['additions'])
    repeated_sizes = {}
    for clause, (_, count) in seen.items():
        if count > 1:
            size = len(clause) // 4
            repeated_sizes[size] = repeated_sizes.get(size, 0) + count - 1
    output = dict(repeated_sizes=repeated_sizes, repeated_unique=sum(count > 1 for _, count in seen.values()),
                  journal_sha256=hashlib.sha256(data).hexdigest(),
                  comparison='Exact sorted literal bytes; dictionary hash collisions do not merge clauses.',
                  caveat='The shared journal omits deletions. Repeated additions alone do not prove a '
                         'clause had been deleted. Prior UNSAT queries also add globally entailed '
                         'blocking clauses, so additions are not exactly conflict counts.', rows=rows)
    a.output.write_text(json.dumps(output, indent=2) + '\n')
    print(json.dumps(rows, indent=2))


if __name__ == '__main__':
    main()

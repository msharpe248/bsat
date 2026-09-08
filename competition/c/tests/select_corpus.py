#!/usr/bin/env python3
"""Select distinct-family CNFs in a size range, excluding recorded JSON history."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import random


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def select(dataset, reports, output, families, max_bytes=0, min_bytes=0, seed=None):
    if families <= 0 or max_bytes < 0 or min_bytes < 0:
        raise ValueError('families must be positive and byte limits nonnegative')
    if max_bytes and min_bytes > max_bytes:
        raise ValueError('min-bytes must not exceed max-bytes')
    dataset, reports, output = dataset.resolve(), reports.resolve(), output.resolve()
    if not dataset.is_dir() or not reports.is_dir():
        raise ValueError('dataset and reports must be existing directories')
    names, hashes, history = set(), set(), []
    for report in sorted(reports.glob('*.json')):
        if report.resolve() == output:
            continue
        text = report.read_text()
        json.loads(text)  # Fail closed on malformed history, regardless of schema.
        names.update(re.findall(r'[\w.-]+\.cnf\b', text))
        hashes.update(re.findall(r'\b[0-9a-fA-F]{64}\b', text.lower()))
        history.append({'path': str(report), 'sha256': digest(report)})
    candidates = sorted((p for p in dataset.glob('*/*.cnf') if p.is_file()),
                        key=lambda p: (p.stat().st_size, p.relative_to(dataset).as_posix()))
    if seed is not None:
        # Shuffle families first, so families with many files do not get more
        # chances to enter the sample. Shuffle files independently within each.
        groups = {}
        for path in sorted(candidates):
            groups.setdefault(path.parent.name, []).append(path)
        rng = random.Random(seed)
        names_order = sorted(groups); rng.shuffle(names_order)
        candidates = []
        for family in names_order:
            rng.shuffle(groups[family]); candidates.extend(groups[family])
    selected, used_families, used_hashes = {}, set(), set()
    for path in candidates:
        family, size = path.parent.name, path.stat().st_size
        if (family in used_families or path.name in names or size < min_bytes
                or (max_bytes and size > max_bytes)):
            continue
        sha = digest(path)
        if sha in hashes or sha in used_hashes:
            continue
        selected[str(path)] = {'sha256': sha, 'family': family, 'bytes': size}
        used_families.add(family)
        used_hashes.add(sha)
        if len(selected) == families:
            break
    if len(selected) != families:
        raise ValueError(f'only {len(selected)} eligible distinct families; requested {families}')
    return {'schema': 1, 'policy': ('smallest eligible file first; one per family; path breaks size ties'
                                  if seed is None else 'seeded family shuffle, then file shuffle; first eligible unique content per family'),
            'seed': seed,
            'exclusions': 'CNF filenames and SHA256 strings appearing in JSON history; duplicate selected content',
            'dataset': str(dataset), 'requested_families': families,
            'min_bytes': min_bytes, 'max_bytes': max_bytes,
            'history': history, 'inputs': selected}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--dataset', required=True, type=Path)
    p.add_argument('--reports', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--families', type=int, default=16)
    p.add_argument('--max-bytes', type=int, default=0)
    p.add_argument('--min-bytes', type=int, default=0)
    p.add_argument('--seed', type=int, help='Shuffle families and files reproducibly instead of selecting smallest first')
    args = p.parse_args()
    try:
        result = select(args.dataset, args.reports, args.output, args.families,
                        args.max_bytes, args.min_bytes, args.seed)
    except (ValueError, OSError) as error:
        p.error(str(error))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(f'Selected {len(result["inputs"])} inputs; recorded {len(result["history"])} history hashes')


if __name__ == '__main__':
    main()

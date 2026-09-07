#!/usr/bin/env python3
"""Serial parse-only benchmarks with original-input fingerprints and pinned inputs.

Build with make parse-benchmark; supply each build with --parser NAME=PATH. This measures loading, not SAT solving or proofs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import random
import statistics
import subprocess


def digest(path):
    h = hashlib.sha256()
    with open(path, 'rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            h.update(block)
    return h.hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--parser', action='append', required=True)
    p.add_argument('--repeats', type=int, default=3)
    p.add_argument('--seed', type=int, default=20260917)
    p.add_argument('--timeout', type=float, default=60)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('inputs', nargs='+', type=Path)
    args = p.parse_args()
    if args.repeats < 1 or args.timeout <= 0:
        p.error('repeats and timeout must be positive')
    parsers = {}
    for spec in args.parser:
        name, path = spec.split('=', 1)
        if not name or name in parsers:
            p.error('parser names must be nonempty and unique')
        path = str(Path(path).resolve())
        parsers[name] = dict(path=path, sha256=digest(path))
    inputs = [dict(path=str(x.resolve()), bytes=x.stat().st_size, sha256=digest(x)) for x in args.inputs]
    revision = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    dirty = bool(subprocess.run(['git', 'diff', '--quiet']).returncode)
    report = dict(revision=revision, dirty=dirty, platform=platform.platform(), machine=platform.machine(), seed=args.seed,
                  repeats=args.repeats, timeout=args.timeout, parsers=parsers, inputs=inputs, runs=[])
    jobs = [(name, inp['path'], repeat) for name in parsers for inp in inputs for repeat in range(args.repeats)]
    random.Random(args.seed).shuffle(jobs)
    expected = {}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    for name, path, repeat in jobs:
        run = subprocess.run([parsers[name]['path'], path], capture_output=True, text=True, timeout=args.timeout)
        if run.returncode:
            raise RuntimeError((name, path, run.returncode, run.stdout, run.stderr))
        result = json.loads(run.stdout)
        if result['status'] != 0:
            raise RuntimeError((name, path, result))
        signature = tuple(result[k] for k in ('variables', 'clauses', 'input_words', 'fingerprint'))
        if path in expected and signature != expected[path]:
            raise RuntimeError(f'Original input differs for {name}: {path}')
        expected[path] = signature
        report['runs'].append(dict(parser=name, input=path, repeat=repeat, **result))
        args.output.write_text(json.dumps(report, indent=2)+'\n')
        print(name, Path(path).parent.name, result['parse_cpu_seconds'], flush=True)
    report['summary'] = {name: dict(runs=sum(r['parser'] == name for r in report['runs']),
                        median_parse_cpu=statistics.median(r['parse_cpu_seconds'] for r in report['runs'] if r['parser'] == name))
                         for name in parsers}
    args.output.write_text(json.dumps(report, indent=2)+'\n')
    print('PASS: all original-input fingerprints match', flush=True)


if __name__ == '__main__':
    main()

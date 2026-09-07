#!/usr/bin/env python3
"""Independent truth-table, original-model and RUP-certificate validation.

Uses no BSAT parser or solver code. Saves minimized failing CNFs and exact
commands. Optional --checker invokes drat-trim on every UNSAT certificate.
"""
import argparse
import itertools
import json
import random
import subprocess
import tempfile
from pathlib import Path


def parse_cnf(text):
    clauses, clause = [], []
    n = None
    for line in text.splitlines():
        if not line.strip() or line.lstrip().startswith('c'):
            continue
        if line.startswith('p '):
            _, kind, n, count = line.split()
            assert kind == 'cnf'
            n, count = int(n), int(count)
            continue
        for token in line.split():
            lit = int(token)
            if lit:
                clause.append(lit)
            else:
                clauses.append(clause)
                clause = []
    assert n is not None and not clause and len(clauses) == count
    return n, clauses


def cnf_text(n, clauses):
    return f'p cnf {n} {len(clauses)}\n' + ''.join(' '.join(map(str, c)) + ' 0\n' for c in clauses)


def truth(n, clauses):
    return any(all(any(bits[abs(lit)-1] == (lit > 0) for lit in c) for c in clauses)
               for bits in itertools.product((False, True), repeat=n))


def model_valid(clauses, output):
    values = {}
    for line in output.splitlines():
        if line.startswith('v '):
            for token in line[2:].split():
                lit = int(token)
                if lit:
                    if abs(lit) in values and values[abs(lit)] != (lit > 0):
                        return False
                    values[abs(lit)] = lit > 0
    return all(any(abs(lit) in values and values[abs(lit)] == (lit > 0) for lit in c) for c in clauses)


def rup(clauses, candidate):
    values = {}
    for lit in candidate:
        if abs(lit) in values and values[abs(lit)] == (lit > 0):
            return True  # negated candidate is contradictory
        values[abs(lit)] = lit < 0
    changed = True
    while changed:
        changed = False
        for clause in clauses:
            c = set(clause)
            if any(-lit in c for lit in c):
                continue
            if any(abs(lit) in values and values[abs(lit)] == (lit > 0) for lit in c):
                continue
            pending = [lit for lit in c if abs(lit) not in values]
            if not pending:
                return True
            if len(pending) == 1:
                lit = pending[0]
                values[abs(lit)] = lit > 0
                changed = True
    return False


def proof_steps(data, binary=False):
    if not binary:
        for line in data.decode().splitlines():
            tokens = line.split()
            deletion = tokens[0] == 'd'
            literals = list(map(int, tokens[1:] if deletion else tokens))
            assert literals[-1] == 0
            yield deletion, literals[:-1]
    else:
        at = 0
        while at < len(data):
            marker = data[at]
            assert marker in (ord('a'), ord('d'))
            at += 1
            clause = []
            while data[at]:
                value, shift = 0, 0
                while True:
                    byte = data[at]
                    at += 1
                    value |= (byte & 127) << shift
                    if byte < 128:
                        break
                    shift += 7
                clause.append(-(value >> 1) if value & 1 else value >> 1)
            at += 1
            yield marker == ord('d'), clause


def verify_proof(clauses, data, binary=False, require_empty=True):
    active = [frozenset(c) for c in clauses]
    empty = False
    for deletion, c in proof_steps(data, binary):
        c = frozenset(c)
        if deletion:
            if c in active:
                active.remove(c)
        else:
            assert rup(active, c), f'non-RUP clause {sorted(c)}'
            active.append(c)
            empty |= not c
    assert empty or not require_empty, 'missing empty clause'


def run_case(solver, n, clauses, options, directory, checker=None, text=None):
    inp, proof = directory/'input.cnf', directory/'proof.drat'
    inp.write_text(text if text is not None else cnf_text(n, clauses))
    cmd = [solver, *options, '--proof', str(proof), str(inp)]
    expected = truth(n, clauses)
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
        assert r.returncode == (10 if expected else 20), (r.returncode, r.stdout, r.stderr)
        assert 'runtime error:' not in r.stderr and 'AddressSanitizer' not in r.stderr, r.stderr
        if expected:
            assert model_valid(clauses, r.stdout), f'invalid model: {r.stdout}'
            verify_proof(clauses, proof.read_bytes(), '--binary-proof' in options, require_empty=False)
        else:
            verify_proof(clauses, proof.read_bytes(), '--binary-proof' in options)
            if checker:
                checked = subprocess.run([checker, str(inp), str(proof)], capture_output=True, text=True, timeout=20)
                assert 's VERIFIED' in checked.stdout, checked.stdout + checked.stderr
    except (AssertionError, subprocess.TimeoutExpired) as exc:
        return str(exc)
    return None


def check_cli(solver, directory):
    inp = directory/'cli.cnf'
    inp.write_text('p cnf 1 1\n1 0\n')
    for options in (['--reduce-interval', '0'], ['--glucose-window-size', '0'],
                    ['--seed', '-1'], ['--time', 'nan'], ['--time', 'junk'],
                    ['--minimize-budget', '-1'], ['--minimize-budget', '4294967296'],
                    ['--equiv-budget', '-1'], ['--equiv-budget', '18446744073709551616'],
                    ['--glucose-k', '2'], ['--proof', str(inp)],
                    ['--proof', str(directory/'absent'/'proof')]):
        result = subprocess.run([solver, *options, str(inp)], capture_output=True, text=True, timeout=5)
        assert result.returncode == 1, (options, result.returncode, result.stderr)
    assert inp.read_text() == 'p cnf 1 1\n1 0\n'
    for malformed in ('p cnf 1 1\n1\0x 0\n', 'p cnf\0x 1 1\n1 0\n'):
        inp.write_text(malformed)
        result = subprocess.run([solver, str(inp)], capture_output=True, text=True, timeout=5)
        assert result.returncode == 1 and not any(line.startswith('s ') for line in result.stdout.splitlines()), result
    inp.write_text('p cnf 4 2\n1 2 0\n3 4 0\n')
    result = subprocess.run([solver, '--no-probing', '--decisions', '1', str(inp)], capture_output=True, text=True, timeout=5)
    assert result.returncode == 0 and 's UNKNOWN' in result.stdout, result


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--solver', default='bin/bsat')
    p.add_argument('--checker')
    p.add_argument('--cases', type=int, default=200)
    p.add_argument('--seed', type=int, default=20260906)
    p.add_argument('--failures', type=Path, default=Path('validation-failures'))
    args = p.parse_args()
    solver = str(Path(args.solver).resolve())
    rng = random.Random(args.seed)
    configs = [[], ['--no-probing'], ['--elim'], ['--bce'],
               ['--elim', '--bce'], ['--glucose-restart-avg', '--no-probing'],
               ['--luby-restart', '--luby-unit', '1', '--no-probing'],
               ['--inprocess', '--inprocess-interval', '1', '--no-probing', '--glucose-min-conflicts', '1'],
               ['--reduce-interval', '1', '--glue-lbd', '0', '--max-lbd', '1', '--no-probing'],
               ['--local-search', '--ls-interval', '1', '--no-probing'],
               ['--alternating', '--no-circular', '--no-probing'], ['--binary-proof', '--elim'],
               ['--iterative-minimize', '--minimize-budget', '1', '--no-probing'],
               ['--iterative-minimize', '--minimize-budget', '0', '--no-probing']]
    configs += [['--iterative-minimize'] + options for options in configs[:12]]
    configs += [['--minimize-budget', '0', '--no-probing'],
                ['--minimize-budget', '1', '--no-probing']]
    configs += [['--equiv'], ['--equiv', '--binary-proof'],
                ['--equiv', '--elim', '--bce'], ['--equiv', '--equiv-budget', '1'],
                ['--equiv', '--equiv-budget', '64'], ['--equiv', '--equiv-budget', '128']]
    configs += [['--dynamic-lbd'],
                ['--dynamic-lbd', '--reduce-interval', '1', '--reduce-fraction', '0'],
                ['--dynamic-lbd', '--binary-proof', '--equiv', '--elim', '--bce']]
    configs += [['--time', '5'],
                ['--time', '5', '--iterative-minimize', '--equiv', '--elim', '--bce'],
                ['--time', '5', '--inprocess', '--inprocess-interval', '1',
                 '--reduce-interval', '1', '--dynamic-lbd', '--binary-proof']]
    configs += [['--protect-used'],
                ['--protect-used', '--binary-proof', '--reduce-interval', '1', '--max-lbd', '3'],
                ['--protect-used', '--dynamic-lbd', '--equiv', '--elim', '--bce',
                 '--inprocess', '--inprocess-interval', '1']]
    configs += [['--vmtf'],
                ['--vmtf', '--protect-used', '--dynamic-lbd', '--reduce-interval', '1', '--binary-proof'],
                ['--vmtf', '--equiv', '--elim', '--bce', '--inprocess', '--inprocess-interval', '1'],
                ['--vmtf', '--local-search', '--ls-interval', '1', '--no-probing', '--iterative-minimize']]
    configs += [['--binary-minimize'], ['--binary-minimize', '--vmtf'],
                ['--binary-minimize', '--binary-proof', '--reduce-interval', '1', '--no-probing'],
                ['--binary-minimize', '--equiv', '--elim', '--bce', '--inprocess', '--inprocess-interval', '1'],
                ['--binary-minimize', '--iterative-minimize'],
                ['--binary-minimize', '--minimize-budget', '1'],
                ['--binary-minimize', '--minimize-budget', '0']]
    configs += [['--random-phase'],
                ['--random-phase', '--vmtf', '--binary-proof'],
                ['--random-phase', '--equiv', '--elim', '--bce'],
                ['--random-phase', '--inprocess', '--inprocess-interval', '1',
                 '--local-search', '--ls-interval', '1', '--no-probing']]
    fixed = [
        (5, [[1, 2], [1, -2], [-3, 4], [3, -4], [4, 5]], None),
        (5, [[-4], [-1, 2, 4], [1, -2, 4]] +
         [[(-1 if bits & 1 else 1)*2, (-1 if bits & 2 else 1)*3,
           (-1 if bits & 4 else 1)*5] for bits in range(8)], None),
        (4, [[-1, 2], [1, -2]] +
         [[(-1 if bits & 1 else 1)*2, (-1 if bits & 2 else 1)*3,
           (-1 if bits & 4 else 1)*4] for bits in range(8)], None),
        (4, [[1, 2], [-1, -2]] +
         [[(-1 if bits & 1 else 1)*2, (-1 if bits & 2 else 1)*3,
           (-1 if bits & 4 else 1)*4] for bits in range(8)], None),
        (3, [[-1, 2], [1, -2], [2, 3]], None),
        (3, [[1, 2], [-1, -2], [2, 3]], None),
        (2, [[1, 2], [-1, 2], [1, -2], [-1, -2]], None),
        (1, [[]], 'p cnf 1 1\n0\n'),
        (1, [[1], [-1]], 'p cnf 1 2\n1 0 -1 0\n'),
        (2, [[1, 2], [-1]], 'p cnf 2 2\n1\n2 0\n-1 0\n'),
        (6, [[-4, 5, 3], [-3, -4, -5]], None),
        (0, [], 'p cnf 0 0\n'),
        (3, [[1, 1], [2, -2, 3], [-1, -1]], None),
    ]
    total = 0
    with tempfile.TemporaryDirectory(prefix='bsat-validation-') as tmp:
        directory = Path(tmp)
        check_cli(solver, directory)
        for i in range(args.cases + len(fixed)):
            if i < len(fixed):
                n, clauses, text = fixed[i]
            else:
                n = rng.randint(2, 9)
                clauses = [[v if rng.randrange(2) else -v
                            for v in rng.sample(range(1, n+1), rng.randint(1 if i % 4 == 0 else 2, min(4,n)))]
                           for _ in range(rng.randint(n, 6*n))]
                text = None
            for options in configs:
                failure = run_case(solver, n, clauses, options, directory, args.checker, text)
                total += 1
                if failure:
                    minimized = clauses[:]
                    for c in clauses:
                        trial = minimized[:]
                        trial.remove(c)
                        if run_case(solver, n, trial, options, directory, args.checker):
                            minimized = trial
                    args.failures.mkdir(parents=True, exist_ok=True)
                    (args.failures/'original.cnf').write_text(text or cnf_text(n,clauses))
                    (args.failures/'minimized.cnf').write_text(cnf_text(n,minimized))
                    (args.failures/'failure.json').write_text(json.dumps({'options':options, 'seed':args.seed, 'case':i, 'failure':failure},indent=2))
                    raise SystemExit(f'FAIL {options}: {failure}\nSaved to {args.failures}')
    print(f'PASS: {total} solves; truth-table answers, original models, text/binary RUP proofs checked (seed {args.seed})')


if __name__ == '__main__':
    main()

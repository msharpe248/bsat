#!/usr/bin/env python3
"""Long public-ABI histories compared with an independent solver on exact snapshots."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import subprocess
import tempfile
from public_api import Cancel, library, literals
from validate import model_valid
from verified_check import verify


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--library', required=True, type=Path)
    p.add_argument('--reference', required=True, type=Path, help='Kissat executable')
    p.add_argument('--queries', type=int, default=256, help='Queries per flag combination')
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args()
    if a.queries < 1: p.error('queries must be positive')
    lib = library(a.library.resolve())
    converter, checker = os.environ['BSAT_DRAT_TRIM'], os.environ['BSAT_CAKE_LPR']
    sha = lambda f: hashlib.sha256(Path(f).read_bytes()).hexdigest()
    report = dict(scope=__doc__, library_sha256=sha(a.library), reference_sha256=sha(a.reference),
                  converter_sha256=sha(converter),checker_sha256=sha(checker),
                  seed=2026090834, complete=False, runs=[], cancellations=0, checkpoints=0, slices=0)
    a.output.parent.mkdir(parents=True, exist_ok=True)
    for flags in range(4):
        rng = random.Random(report['seed']) # Replay identical histories across modes.
        s = lib.bsat_create(1, flags); assert s
        stop = [0]
        callback = Cancel(lambda _: stop[0])
        lib.bsat_set_terminate(s, None, callback)
        clauses = []
        def add(c):
            assert lib.bsat_add_clause(s, literals(c), len(c))
            clauses.append(c)
        # Planted 3-CNF stays satisfiable before assumptions; random queries can be UNSAT.
        n = 128
        for v in range(1, n + 1): add([v, -v])
        def grow():
            c = [v * rng.choice([-1, 1]) for v in rng.sample(range(1, n + 1), 3)]
            if all(v < 0 for v in c): c[0] = -c[0]
            add(c)
        for _ in range(400): grow()
        try:
            for q in range(a.queries):
                if q % 4 == 0: grow()
                assumptions = [v * rng.choice([-1, 1]) for v in rng.sample(range(1, n + 1), q % 13)]
                if q % 19 == 0: assumptions += [1, -1]
                arr = literals(assumptions)
                assert lib.bsat_set_query_limits(s, 5, 0, 0)
                if q % 17 == 0:
                    stop[0] = 1
                    assert lib.bsat_solve(s, arr, len(arr)) == 0
                    assert not lib.bsat_value(s, 1)
                    stop[0] = 0; report['cancellations'] += 1
                if q % 31 == 0:
                    assert lib.bsat_checkpoint(s); report['checkpoints'] += 1
                    assert not lib.bsat_value(s, 1)
                sliced = 0
                if q % 23 == 0:
                    assert lib.bsat_set_query_limits(s, 5, 1, 0)
                    sliced = lib.bsat_solve(s, arr, len(arr))
                    assert sliced in (0, 10, 20) and not lib.bsat_error(s)
                    report['slices'] += sliced == 0
                    assert lib.bsat_set_query_limits(s, 5, 0, 0)
                result = lib.bsat_solve(s, arr, len(arr))
                assert result in (10, 20) and not lib.bsat_error(s), (flags, q, result)
                assert sliced in (0, result), (flags, q, sliced, result)
                exact = clauses + [[v] for v in assumptions]
                with tempfile.TemporaryDirectory(prefix='bsat-differential-') as temp:
                    root = Path(temp); cnf = root/'reference.cnf'; proof = root/'reference.drat'
                    cnf.write_text(f'p cnf {n} {len(exact)}\n' + ''.join(' '.join(map(str,c))+' 0\n' for c in exact))
                    ref = subprocess.run([str(a.reference.resolve()), str(cnf), str(proof)], capture_output=True, text=True, timeout=10)
                    assert ref.returncode == result, (flags, q, result, ref.stdout, ref.stderr)
                    row = dict(flags=flags, query=q, clauses=len(clauses), assumptions=assumptions,
                               input_sha256=sha(cnf), result=result)
                    if result == 10:
                        model = 'v ' + ' '.join(str(lib.bsat_value(s, v)) for v in range(1, n+1)) + ' 0\n'
                        assert model_valid(exact, model) and model_valid(exact, ref.stdout)
                    else:
                        checked = verify(cnf, proof, converter, checker, root/'reference-check', 30)
                        assert checked['verified']; row['reference_proof_sha256'] = checked['drat_sha256']
                        core = [v for v in assumptions if lib.bsat_failed(s, v)]
                        core_cnf, core_proof = root/'core.cnf', root/'core.drat'
                        core_clauses = clauses + [[v] for v in core]
                        core_cnf.write_text(f'p cnf {n} {len(core_clauses)}\n' + ''.join(' '.join(map(str,c))+' 0\n' for c in core_clauses))
                        core_ref = subprocess.run([str(a.reference.resolve()), str(core_cnf), str(core_proof)],capture_output=True,text=True,timeout=10)
                        assert core_ref.returncode == 20, (flags,q,core,core_ref.stdout)
                        checked = verify(core_cnf,core_proof,converter,checker,root/'core-check',30)
                        assert checked['verified'];row['failed_core'] = core
                        row['core_proof_sha256'] = checked['drat_sha256']
                    if flags & 2:
                        export, journal = root/'export.cnf', root/'export.drat'
                        assert lib.bsat_export_query(s, os.fsencode(export), os.fsencode(journal))
                        # Verify the exported context itself, not just its declared status.
                        from validate import parse_cnf
                        _, exported = parse_cnf(export.read_text())
                        assert exported == exact
                        if result == 20:
                            checked = verify(export, journal, converter, checker, root/'bsat-check', 30)
                            assert checked['verified']; row['bsat_proof_sha256'] = checked['drat_sha256']
                        else: assert model_valid(exported, model)
                    row['verified'] = True; report['runs'].append(row)
                if q % 64 == 63:
                    a.output.write_text(json.dumps(report, indent=2)+'\n')
                    print('flags', flags, 'queries', q+1, flush=True)
        except Exception as error:
            report['failure'] = dict(flags=flags,query=q,clauses=clauses,assumptions=assumptions,error=repr(error))
            a.output.write_text(json.dumps(report,indent=2)+'\n')
            raise
        finally: lib.bsat_destroy(s)
    report['complete'] = True
    a.output.write_text(json.dumps(report, indent=2)+'\n')
    print('PASS:', len(report['runs']), 'independent differential queries', flush=True)


if __name__ == '__main__': main()

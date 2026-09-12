"""Compare certified search profiles on pinned CNFs; independently check answers."""
import argparse
import ctypes as C
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import tempfile
import time

from public_api import library, literals, Stats
from validate import parse_cnf, model_valid
from verified_check import verify


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profiles', default='control,alternating,vsids')
    parser.add_argument('--flags', type=int, default=3)
    parser.add_argument('--cpu', type=float, default=30)
    parser.add_argument('--conflicts', type=int, default=0)
    parser.add_argument('--accounting', action='store_true')
    args = parser.parse_args()
    if not math.isfinite(args.cpu) or args.cpu <= 0:
        parser.error('CPU limit must be positive and finite')
    if not 0 <= args.conflicts < 2**32:
        parser.error('conflict limit must fit uint32; zero means CPU-only')
    lib = library(args.library.resolve())
    diagnostic = hasattr(lib, 'bsat_diagnostic_configure')
    if args.accounting and not diagnostic:
        parser.error('accounting requires a diagnostic library')
    if diagnostic:
        lib.bsat_diagnostic_configure.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
        lib.bsat_diagnostic_configure.restype = C.c_int
        lib.bsat_diagnostic_begin_query.argtypes = [C.c_void_p]
        lib.bsat_diagnostic_begin_query.restype = None
        lib.bsat_diagnostic_write.argtypes = [C.c_void_p, C.c_char_p]
        lib.bsat_diagnostic_write.restype = C.c_int
    elif args.profiles != 'control':
        parser.error('option profiles require the diagnostic library')
    manifest = json.loads(args.manifest.read_text())
    report = dict(complete=False, platform=platform.platform(), cpu=args.cpu,
                  conflicts=args.conflicts, accounting=args.accounting,
                  flags=args.flags, harness_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  library_sha256=hashlib.sha256(args.library.read_bytes()).hexdigest(),
                  manifest_sha256=hashlib.sha256(args.manifest.read_bytes()).hexdigest(),
                  profiles=args.profiles.split(','), inputs=manifest['inputs'], runs=[],
                  timing='Serial process CPU for solve; parsing, addition, export and independent checking excluded.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    for filename, metadata in manifest['inputs'].items():
        source = Path(filename)
        assert hashlib.sha256(source.read_bytes()).hexdigest() == metadata['sha256']
        variables, clauses = parse_cnf(source.read_text())
        for profile in report['profiles']:
            solver = lib.bsat_create(1, args.flags)
            assert solver
            try:
                if diagnostic:
                    for option in profile.split('+'):
                        assert lib.bsat_diagnostic_configure(solver, option.encode(), args.accounting)
                for clause in clauses:
                    assert lib.bsat_add_clause(solver, literals(clause), len(clause))
                assert lib.bsat_set_query_limits(solver, args.cpu, args.conflicts, 0)
                if diagnostic:
                    lib.bsat_diagnostic_begin_query(solver)
                start = time.process_time()
                result = lib.bsat_solve(solver, literals([]), 0)
                elapsed = time.process_time() - start
                assert result in (0, 10, 20) and not lib.bsat_error(solver)
                stats = Stats()
                assert lib.bsat_get_stats(solver, C.byref(stats), C.sizeof(stats))
                row = dict(input=filename, input_sha256=metadata['sha256'], profile=profile,
                           result=result, cpu_seconds=elapsed, conflicts=stats.conflicts,
                           decisions=stats.decisions, propagations=stats.propagations,
                           verified=False)
                if diagnostic:
                    with tempfile.TemporaryDirectory(prefix='bsat-cnf-diagnostics-') as temp:
                        snapshot = Path(temp) / 'diagnostic.json'
                        assert lib.bsat_diagnostic_write(solver, os.fsencode(snapshot))
                        row['diagnostic'] = json.loads(snapshot.read_text())
                        if args.accounting:
                            assert row['diagnostic'].get('accounting_available'), (
                                'accounting requires a BSAT_SEARCH_DIAGNOSTICS build')
                if args.conflicts:
                    assert elapsed < args.cpu and (result or stats.conflicts == args.conflicts)
                if result == 10:
                    model = 'v ' + ' '.join(str(-v if lib.bsat_value(solver, v) < 0 else v)
                                           for v in range(1, variables + 1)) + ' 0\n'
                    assert model_valid(clauses, model)
                    row['verified'] = True
                elif result == 20:
                    with tempfile.TemporaryDirectory(prefix='bsat-cnf-search-') as temp:
                        folder = Path(temp)
                        cnf, proof = folder / 'query.cnf', folder / 'query.drat'
                        assert lib.bsat_export_query(solver, os.fsencode(cnf), os.fsencode(proof))
                        row['proof_bytes'] = proof.stat().st_size
                        row['validation'] = verify(source, proof, os.environ['BSAT_DRAT_TRIM'],
                                                   os.environ['BSAT_CAKE_LPR'], folder / 'check',
                                                   600, heap_mb=2048, stack_mb=512)
                        assert row['validation']['verified']
                        row['verified'] = True
                report['runs'].append(row)
                args.output.write_text(json.dumps(report, indent=2) + '\n')
                print(metadata['family'], profile, result, round(elapsed, 3), flush=True)
            finally:
                lib.bsat_destroy(solver)
    report['complete'] = True
    args.output.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()

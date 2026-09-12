#!/usr/bin/env python3
"""Replay checked circuit contexts under bounded search, verifying new certificates."""
import argparse
import ctypes as C
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import tempfile
import time

from aag_history import Circuit
from public_api import library, literals, Stats
from validate import model_valid, parse_cnf
from verified_check import verify


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('library', 'circuits', 'checked-report', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--circuit', required=True)
    p.add_argument('--flags', type=int, default=3,
                   help='Public session flags; reference contexts remain pinned to flags 3')
    p.add_argument('--journal-output', type=Path, help='Save the final conclusive query proof for offline learning analysis')
    p.add_argument('--profile', choices=('control', 'queue', 'vsids', 'reduce-ternary', 'positive', 'sustained', 'alternating', 'no-rephase', 'fresh', 'fresh-congruence', 'fresh-substitution', 'fresh-elimination', 'iterative', 'glue-three', 'lrb', 'large-db', 'no-reduce', 'dynamic', 'vivify'), default='control')
    p.add_argument('--also-profile', action='append', default=[],
                   help='Apply another test-only option profile before input')
    p.add_argument('--accounting', action='store_true')
    p.add_argument('--causes', action='store_true')
    p.add_argument('--macos-qos', action='store_true',
                   help='Request USER_INITIATED QoS for the benchmark thread on macOS')
    p.add_argument('--conflicts', type=int, default=200000)
    p.add_argument('--cpu', type=float, default=300)
    a = p.parse_args()
    if not 0 <= a.conflicts < 2**32 or not math.isfinite(a.cpu) or a.cpu <= 0:
        p.error('positive finite CPU and uint32 conflict budgets required; zero means CPU-only')
    if a.causes and not a.accounting:
        p.error('--causes requires --accounting and a diagnostic build')
    if a.macos_qos:
        if platform.system() != 'Darwin':
            p.error('--macos-qos requires macOS')
        system = C.CDLL('/usr/lib/libSystem.B.dylib')
        system.pthread_set_qos_class_self_np.argtypes = [C.c_uint, C.c_int]
        system.pthread_set_qos_class_self_np.restype = C.c_int
        if system.pthread_set_qos_class_self_np(0x19, 0):
            p.error('could not request USER_INITIATED QoS')
    checked = json.loads(a.checked_report.read_text())
    assert checked['complete']
    history = [r for r in checked['runs'] if r['circuit'] == a.circuit and
               r['repeat'] == 0 and r['flags'] == 3]
    assert history and all(r['validated'] for r in history)
    manifest = json.loads((a.circuits / 'manifest.json').read_text())
    item = next(i for i in manifest['inputs'] if i['file'] == a.circuit)
    source = a.circuits / item['aag']
    assert sha(source) == item['aag_sha256']
    circuit = Circuit.read(source.read_text())
    converter, checker = os.environ['BSAT_DRAT_TRIM'], os.environ['BSAT_CAKE_LPR']
    lib = library(a.library.resolve())
    diagnostic = hasattr(lib, 'bsat_diagnostic_configure')
    if not diagnostic and (a.profile != 'control' or a.also_profile or a.accounting or a.causes):
        p.error('option profiles and accounting require a diagnostic library')
    if diagnostic:
        lib.bsat_diagnostic_configure.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
        lib.bsat_diagnostic_configure.restype = C.c_int
        lib.bsat_diagnostic_begin_query.argtypes = [C.c_void_p]
        lib.bsat_diagnostic_begin_query.restype = None
        lib.bsat_diagnostic_write.argtypes = [C.c_void_p, C.c_char_p]
        lib.bsat_diagnostic_write.restype = C.c_int
    s = lib.bsat_create(1, a.flags)
    assert s
    if diagnostic:
        assert lib.bsat_diagnostic_configure(s, a.profile.encode(), a.accounting), (
            f'Profile {a.profile!r} is unavailable in {a.library}')
        for profile in a.also_profile:
            assert lib.bsat_diagnostic_configure(s, profile.encode(), a.accounting), profile
    report = dict(complete=False, platform=platform.platform(),
                  flags=a.flags,
                  macos_qos='USER_INITIATED' if a.macos_qos else 'inherited', profile=a.profile, also_profiles=a.also_profile,
                  library_sha256=sha(a.library), harness_sha256=sha(__file__),
                  checked_report_sha256=sha(a.checked_report), circuit=item,
                  converter_sha256=sha(converter), checker_sha256=sha(checker),
                  conflicts=a.conflicts, cpu=a.cpu, accounting=a.accounting,
                  timing_scope='Query process CPU; intrusive accounting is diagnostic only. '
                               'Independent checks and encoding are outside timing. '
                               'No reference solver is rerun; prior report pins exact inputs.',
                  runs=[])
    a.output.parent.mkdir(parents=True, exist_ok=True)

    def save():
        a.output.write_text(json.dumps(report, indent=2) + '\n')

    clauses, frame = [], 0
    try:
        for expected in history:
            depth, polarity = expected['depth'], expected['polarity']
            while frame <= depth:
                for clause in circuit.frame(frame):
                    assert lib.bsat_add_clause(s, literals(clause), len(clause))
                    clauses.append(clause)
                frame += 1
            n = 1 + frame * circuit.maximum
            assumption = polarity * circuit.literal(circuit.output, depth)
            exact = clauses + [[assumption]]
            data = f'p cnf {n} {len(exact)}\n' + ''.join(
                ' '.join(map(str, c)) + ' 0\n' for c in exact)
            digest = hashlib.sha256(data.encode()).hexdigest()
            assert digest == expected['input_sha256']
            assert lib.bsat_set_query_limits(s, a.cpu, a.conflicts, 0)
            if diagnostic:
                lib.bsat_diagnostic_begin_query(s)
            if a.causes:
                lib.bsat_diagnostic_frames.argtypes = [C.c_void_p, C.c_uint32]
                lib.bsat_diagnostic_frames.restype = None
                lib.bsat_diagnostic_frames(s, circuit.maximum)
            start = time.process_time()
            result = lib.bsat_solve(s, literals([assumption]), 1)
            cpu = time.process_time() - start
            error = bool(lib.bsat_error(s))
            if result not in (0, 10, 20) or error:
                report['failure_query'] = dict(depth=depth, polarity=polarity, result=result,
                                               api_error=error, cpu_seconds=cpu, input_sha256=digest)
                raise AssertionError(f'Invalid query result: {report["failure_query"]}')
            assert not result or not expected['result'] or result == expected['result']
            stats = Stats()
            assert lib.bsat_get_stats(s, C.byref(stats), C.sizeof(stats))
            # A CPU timeout would invalidate the fixed-work comparison.
            if a.conflicts:
                assert cpu < a.cpu and (result or stats.conflicts == a.conflicts)
            row = dict(circuit=a.circuit, repeat=0, flags=a.flags, depth=depth, polarity=polarity,
                       result=result, cpu_seconds=cpu, input_sha256=digest,
                       within_cpu_budget=bool(result and cpu <= a.cpu),
                       cpu_overshoot_seconds=max(0, cpu - a.cpu),
                       stats={k: getattr(stats, k) for k, _ in stats._fields_})
            report['runs'].append(row)
            size = C.c_uint64()
            assert lib.bsat_get_journal_bytes(s, C.byref(size))
            row['journal_bytes'] = size.value
            if result == 10:
                values = {v: (lib.bsat_value(s, v) or v) > 0 for v in range(1, n + 1)}
                model = 'v ' + ' '.join(str(v if b else -v) for v, b in values.items()) + ' 0\n'
                assert model_valid(exact, model)
                assert circuit.simulate(values, depth)[depth] == (polarity == 1)
                row['bsat_model_and_simulation'] = True
            with tempfile.TemporaryDirectory(prefix='bsat-search-scaling-') as tmp:
                root = Path(tmp)
                if diagnostic:
                    snapshot = root / 'diagnostic.json'
                    assert lib.bsat_diagnostic_write(s, os.fsencode(snapshot))
                    row['diagnostic'] = json.loads(snapshot.read_text())
                    if a.accounting:
                        assert row['diagnostic'].get('accounting_available'), (
                            'accounting requires a BSAT_SEARCH_DIAGNOSTICS build')
                if a.causes:
                    assert sum(row['diagnostic']['propagation_source']) == stats.propagations
                if result:
                    cnf, proof = root / 'input.cnf', root / 'proof.drat'
                    start = time.process_time()
                    assert lib.bsat_export_query(s, os.fsencode(cnf), os.fsencode(proof))
                    row['export_cpu_seconds'] = time.process_time() - start
                    assert parse_cnf(cnf.read_text())[1] == exact
                    row['export_proof_bytes'] = proof.stat().st_size
                    if a.journal_output and expected is history[-1]:
                        a.journal_output.parent.mkdir(parents=True, exist_ok=True)
                        shutil.copyfile(proof, a.journal_output)
                    if result == 20:
                        check = verify(cnf, proof, converter, checker, root / 'check', 600, 2048, 512)
                        row['certificate_checks'] = {'bsat-check': check}
                        assert check['verified']
                        row['bsat_proof_sha256'] = check['drat_sha256']
                row['validated'] = True
            save()
            print(a.circuit, depth, polarity, result, stats.conflicts, round(cpu, 3), flush=True)
        report['complete'] = True
    except BaseException as error:
        report['failure'] = repr(error)
        raise
    finally:
        lib.bsat_destroy(s)
        save()


if __name__ == '__main__':
    main()

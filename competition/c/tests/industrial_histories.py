#!/usr/bin/env python3
"""Growing BMC histories from real upstream circuits, with independent witnesses."""
import argparse
import ctypes as C
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tempfile
import time
from aag_history import Circuit
from incremental_reference import Reference
from process_control import run_capture
from public_api import library, literals, Stats
from validate import model_valid, parse_cnf
from verified_check import verify


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--library', required=True, type=Path)
    p.add_argument('--reference', required=True, type=Path)
    p.add_argument('--incremental-reference', required=True, type=Path)
    p.add_argument('--circuits', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--depths', default='0,1,2,4,8,16')
    p.add_argument('--flags', default='0,1,2,3')
    p.add_argument('--repeats', type=int, default=1)
    p.add_argument('--circuit',action='append',help='Exact manifest filename; repeat to select a subset')
    p.add_argument('--cpu',type=float,default=5)
    p.add_argument('--reference-cpu',type=float,default=0,help='Optional retained-reference thread CPU budget; 0 keeps historical wall-only policy')
    p.add_argument('--reference-wall',type=float,default=10)
    p.add_argument('--conflicts',type=int,default=0)
    p.add_argument('--profile',choices=['control','no-rephase','alternating','chrono','growing-reduce','probe-default-budget','journal-off'],help='Requires separate test diagnostic library')
    p.add_argument('--accounting',action='store_true',help='Intrusive per-query phase timers, not timing scores')
    p.add_argument('--reference-statistics',action='store_true',help='Require optional pinned CaDiCaL diagnostic extension; reads outside solve timing')
    p.add_argument('--retain-certified',type=Path,help='Retain exact conclusive BSAT exports for proof-cost experiments')
    p.add_argument('--snapshots',type=Path,help='Retain exact CNFs for one-shot comparisons')
    p.add_argument('--checkpoint-every',type=int,default=0,help='Checkpoint after each N completed queries, before the next solve; 0 disables')
    p.add_argument('--fresh-wall',type=float,default=10,help='Validation-only fresh Kissat wall limit')
    p.add_argument('--checker-wall',type=float,default=60,help='Validation-only per-stage checker wall limit')
    p.add_argument('--checker-heap-mb',type=int,default=512,help='Explicit CakeML heap size; larger proofs may need more than the default')
    p.add_argument('--checker-stack-mb',type=int,default=128,help='Explicit CakeML stack size')
    p.add_argument('--retain-unverified',type=Path,help='Retain failed checker inputs/proofs/logs under this directory')
    a = p.parse_args()
    depths = sorted(set(map(int, a.depths.split(','))))
    flags_list = list(map(int, a.flags.split(',')))
    if not depths or depths[0] < 0 or a.repeats < 1: p.error('invalid depths/repeats')
    if any(not math.isfinite(x) or x<0 for x in (a.cpu,a.reference_cpu,a.reference_wall)) or not a.cpu or not a.reference_wall or not 0<=a.conflicts<2**32:
        p.error('invalid query limits')
    if a.accounting and not a.profile:p.error('accounting requires a diagnostic profile')
    if any(f not in (0,1,2,3,6,7) for f in flags_list) or not flags_list:p.error('invalid flags')
    if a.checkpoint_every<0 or any(not math.isfinite(x) or x<=0 for x in (a.fresh_wall,a.checker_wall)):p.error('invalid checkpoint/validation limits')
    if a.checker_heap_mb<=0 or a.checker_stack_mb<=0:p.error('checker heap and stack must be positive')
    if a.profile=='journal-off' and any(not f&2 for f in flags_list):p.error('journal-off requires certificate flags')
    lib = library(a.library.resolve())
    if a.profile:
        lib.bsat_diagnostic_configure.argtypes=[C.c_void_p,C.c_char_p,C.c_int]
        lib.bsat_diagnostic_configure.restype=C.c_int
        lib.bsat_diagnostic_write.argtypes=[C.c_void_p,C.c_char_p]
        lib.bsat_diagnostic_write.restype=C.c_int
        lib.bsat_diagnostic_begin_query.argtypes=[C.c_void_p]
        lib.bsat_diagnostic_begin_query.restype=None
    converter, checker = os.environ['BSAT_DRAT_TRIM'], os.environ['BSAT_CAKE_LPR']
    sha = lambda f: hashlib.sha256(Path(f).read_bytes()).hexdigest()
    manifest = json.loads((a.circuits/'manifest.json').read_text())
    if a.circuit:
        if set(a.circuit)-{x['file'] for x in manifest['inputs']}:p.error('unknown circuit')
        manifest['inputs']=[x for x in manifest['inputs'] if x['file'] in a.circuit]
    report = dict(scope=__doc__, complete=False, platform=platform.platform(),
                  circuits=manifest, library_sha256=sha(a.library), reference_sha256=sha(a.reference),
                  incremental_reference_sha256=sha(a.incremental_reference),
                  converter_sha256=sha(converter), checker_sha256=sha(checker), runs=[],
                  limits=dict(bsat_cpu=a.cpu, bsat_conflicts=a.conflicts,cadical_thread_cpu=a.reference_cpu,cadical_cooperative_wall=a.reference_wall, kissat_wall=a.fresh_wall, checker_stage_wall=a.checker_wall),
                  checkpoint_every=a.checkpoint_every,
                  checker_heap_mb=a.checker_heap_mb,checker_stack_mb=a.checker_stack_mb,harness_sha256=sha(Path(__file__)),
                  diagnostic_profile=a.profile,accounting=a.accounting,
                  timing_scope='Serial API query process CPU excludes encoding, additions, export and checking; repeated modes alternate order.')
    a.output.parent.mkdir(parents=True, exist_ok=True)
    def save(): a.output.write_text(json.dumps(report, indent=2)+'\n')
    try:
        for repeat in range(a.repeats):
            for item in manifest['inputs']:
                source = a.circuits/item['aag']
                assert sha(source) == item['aag_sha256']
                circuit = Circuit.read(source.read_text())
                for flags in flags_list if repeat % 2 == 0 else flags_list[::-1]:
                    s = lib.bsat_create(1, flags); assert s
                    if a.profile:assert lib.bsat_diagnostic_configure(s,a.profile.encode(),a.accounting)
                    inc = Reference(a.incremental_reference.resolve(),a.reference_cpu,a.reference_wall)
                    report['incremental_signature'] = inc.signature
                    clauses = []; frame = 0; query_index = 0
                    try:
                        for depth in depths:
                            start = time.process_time()
                            while frame <= depth:
                                for clause in circuit.frame(frame):
                                    assert lib.bsat_add_clause(s, literals(clause), len(clause))
                                    inc.add(clause); clauses.append(clause)
                                frame += 1
                            add_cpu = time.process_time()-start
                            n = 1+frame*circuit.maximum
                            # Positive is the actual bad-state reachability query;
                            # negative also checks temporary-assumption replacement.
                            for polarity in (1, -1):
                                assumptions = [polarity*circuit.literal(circuit.output, depth)]
                                exact = clauses+[[v] for v in assumptions]
                                arr = literals(assumptions)
                                checkpoint = None
                                if a.checkpoint_every and query_index and query_index%a.checkpoint_every==0:
                                    before=C.c_uint64();after=C.c_uint64()
                                    if flags&2:assert lib.bsat_get_journal_bytes(s,C.byref(before))
                                    start=time.process_time();assert lib.bsat_checkpoint(s)
                                    cost=time.process_time()-start
                                    assert not lib.bsat_value(s,1) and not lib.bsat_error(s)
                                    if flags&2:
                                        assert lib.bsat_get_journal_bytes(s,C.byref(after)) and after.value==0
                                    checkpoint=dict(cpu_seconds=cost,journal_before=before.value,journal_after=after.value)
                                query_index+=1
                                assert lib.bsat_set_query_limits(s, a.cpu, a.conflicts, 0)
                                if a.profile:lib.bsat_diagnostic_begin_query(s)
                                query_wall_start=time.monotonic()
                                start = time.process_time(); result = lib.bsat_solve(s, arr, len(arr))
                                cpu = time.process_time()-start
                                assert result in (0, 10, 20) and not lib.bsat_error(s)
                                bsat_wall=time.monotonic()-query_wall_start
                                reference_before=inc.statistics() if a.reference_statistics else None
                                start = time.process_time(); reference = inc.solve(assumptions)
                                inc_cpu = time.process_time()-start
                                reference_after=inc.statistics() if a.reference_statistics else None
                                assert reference in (0, 10, 20)
                                assert not result or not reference or result == reference
                                row = dict(circuit=item['file'], repeat=repeat, flags=flags, depth=depth,
                                           polarity=polarity, variables=n, clauses=len(clauses), result=result,
                                           incremental_result=reference, cpu_seconds=cpu, incremental_cpu_seconds=inc_cpu,
                                           additions_both_solvers_cpu=add_cpu if polarity == 1 else 0,
                                           assumptions=assumptions,solve_wall_seconds=bsat_wall)
                                if reference_after is not None:
                                    row['reference_statistics']={'before':reference_before,'after':reference_after,'delta':{k:reference_after[k]-reference_before[k] for k in ('conflicts','decisions','propagations')},'scope':'CaDiCaL public cumulative counters sampled outside query timing; propagation means search propagation only'}
                                if checkpoint is not None:row['checkpoint']=checkpoint
                                row['bsat_cpu_overshoot_seconds']=max(0,cpu-a.cpu)
                                row['reference_cpu_overshoot_seconds']=max(0,inc_cpu-a.reference_cpu) if a.reference_cpu else 0
                                # Keep actual conclusive answers for validation, but never
                                # award a timed solve observed beyond its allowance.
                                row['within_cpu_budget']=bool(result and cpu<=a.cpu)
                                row['reference_within_cpu_budget']=bool(reference and (not a.reference_cpu or inc_cpu<=a.reference_cpu))
                                report['runs'].append(row)
                                stats = Stats(); assert lib.bsat_get_stats(s, C.byref(stats), C.sizeof(stats))
                                row['stats'] = {name: getattr(stats, name) for name, _ in stats._fields_}
                                if flags & 2 and a.profile!='journal-off':
                                    size = C.c_uint64(); assert lib.bsat_get_journal_bytes(s, C.byref(size))
                                    row['journal_bytes'] = size.value
                                def check_model(model):
                                    assert model_valid(exact, model)
                                    values = {abs(v): v > 0 for line in model.splitlines() if line.startswith('v ')
                                              for v in map(int, line.split()[1:]) if v}
                                    # Unmentioned unused signals may be completed arbitrarily.
                                    values = {v: values.get(v, True) for v in range(1, n+1)}
                                    assert circuit.simulate(values, depth)[depth] == (polarity == 1)
                                if result == 10:
                                    model = 'v '+' '.join(str(lib.bsat_value(s, v) or v) for v in range(1, n+1))+' 0\n'
                                    check_model(model); row['bsat_model_and_simulation'] = True
                                if reference == 10:
                                    check_model(inc.model(n)); row['incremental_model_and_simulation'] = True
                                with tempfile.TemporaryDirectory(prefix='bsat-industrial-') as tmp:
                                    root = Path(tmp); cnf = root/'input.cnf'; proof = root/'proof.drat'
                                    def check_certificate(input_path,proof_path,tag):
                                        failure=None;folder=root/tag
                                        try:
                                            checked=verify(input_path,proof_path,converter,checker,folder,a.checker_wall,a.checker_heap_mb,a.checker_stack_mb)
                                        except (OSError,subprocess.TimeoutExpired) as error:
                                            failure=error
                                            log=folder/'verification.json'
                                            checked=json.loads(log.read_text()) if log.exists() else dict(verified=False,error=repr(error))
                                        row.setdefault('certificate_checks',{})[tag]=checked
                                        if not checked['verified']:
                                            if a.retain_unverified and folder.exists():
                                                a.retain_unverified.mkdir(parents=True,exist_ok=True)
                                                target=Path(tempfile.mkdtemp(prefix=tag+'-',dir=a.retain_unverified))
                                                shutil.copytree(folder,target,dirs_exist_ok=True)
                                                row['unverified_artifacts']=str(target.resolve())
                                            save()
                                            if failure is not None:raise failure
                                            raise AssertionError(f'{tag}: certificate check failed; see certificate_checks in report')
                                        return checked
                                    cnf.write_text(f'p cnf {n} {len(exact)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in exact))
                                    if a.profile:
                                        diagnostic=root/'diagnostic.json'
                                        assert lib.bsat_diagnostic_write(s,os.fsencode(diagnostic))
                                        row['diagnostic']=json.loads(diagnostic.read_text())
                                    if a.snapshots:
                                        a.snapshots.mkdir(parents=True,exist_ok=True)
                                        target=a.snapshots/f'{source.stem}-depth{depth}-polarity{polarity}.cnf'
                                        if target.exists():assert target.read_bytes()==cnf.read_bytes()
                                        else:target.write_bytes(cnf.read_bytes())
                                    row['input_sha256'] = sha(cnf)
                                    try:
                                        fresh = run_capture([str(a.reference.resolve()), str(cnf), str(proof)], a.fresh_wall)
                                        row['fresh_result'] = fresh.returncode
                                        assert fresh.returncode in (0, 10, 20), fresh.stderr
                                    except subprocess.TimeoutExpired:
                                        fresh = None; row['fresh_result'] = 0; row['fresh_timeout'] = True
                                    fresh_result = row['fresh_result']
                                    for r in (result, reference):
                                        assert not r or not fresh_result or r == fresh_result
                                    if fresh_result == 10:
                                        check_model(fresh.stdout); row['fresh_model_and_simulation'] = True
                                    elif fresh_result == 20:
                                        checked = check_certificate(cnf, proof, 'fresh-check')
                                        assert checked['verified']; row['fresh_proof_sha256'] = checked['drat_sha256']
                                    if result and flags & 2 and a.profile!='journal-off':
                                        export, journal = root/'export.cnf', root/'export.drat'
                                        start = time.process_time()
                                        assert lib.bsat_export_query(s, os.fsencode(export), os.fsencode(journal))
                                        row['export_cpu_seconds'] = time.process_time()-start
                                        row['export_proof_bytes']=journal.stat().st_size
                                        if a.retain_certified:
                                            folder=a.retain_certified/f'{source.stem}-r{repeat}-f{flags}-d{depth}-p{polarity}'
                                            folder.mkdir(parents=True,exist_ok=False)
                                            shutil.copyfile(export,folder/'input.cnf');shutil.copyfile(journal,folder/'proof.drat')
                                        _, exported = parse_cnf(export.read_text()); assert exported == exact
                                        if result == 20:
                                            checked = check_certificate(export, journal, 'bsat-check')
                                            assert checked['verified']; row['bsat_proof_sha256'] = checked['drat_sha256']
                                        else: assert model_valid(exported, model)
                                    # A lone uncertified UNSAT is not accepted as validated.
                                    if result == 20 or reference == 20:
                                        assert row.get('fresh_proof_sha256') or row.get('bsat_proof_sha256')
                                row['query_validation_wall_seconds']=time.monotonic()-query_wall_start
                                row['query_validation_scope']='BSAT solve, retained reference solve, model/simulation, fresh reference, export and proof checks; excludes encoding/additions'
                                row['validated'] = True; save()
                            print(item['file'], 'flags', flags, 'depth', depth, 'checked', flush=True)
                    finally:
                        lib.bsat_destroy(s); inc.close()
        report['complete'] = True
    except BaseException as error:
        report['failure'] = repr(error); raise
    finally: save()
    print('PASS:', len(report['runs']), 'industrial circuit queries', flush=True)


if __name__ == '__main__': main()

#!/usr/bin/env python3
"""Compare fresh certified assumption/unit encodings of the same exact query."""
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
from aag_history import Circuit
from public_api import library, literals, Stats
from validate import parse_cnf, model_valid
from verified_check import verify


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('library','circuits','output'):
        p.add_argument('--'+name,required=True,type=Path)
    p.add_argument('--circuit',action='append',required=True)
    p.add_argument('--depth',type=int,default=16)
    p.add_argument('--cpu',type=float,default=60)
    p.add_argument('--conflicts',type=int,default=0)
    p.add_argument('--repeats',type=int,default=2)
    p.add_argument('--accounting',action='store_true')
    p.add_argument('--profile',default='control')
    a=p.parse_args()
    if a.depth<0 or not math.isfinite(a.cpu) or a.cpu<=0 or a.repeats<1 or not 0<=a.conflicts<2**32:p.error('invalid limits')
    lib=library(a.library.resolve())
    lib.bsat_diagnostic_configure.argtypes=[C.c_void_p,C.c_char_p,C.c_int]
    lib.bsat_diagnostic_configure.restype=C.c_int
    lib.bsat_diagnostic_begin_query.argtypes=[C.c_void_p]
    lib.bsat_diagnostic_begin_query.restype=None
    lib.bsat_diagnostic_write.argtypes=[C.c_void_p,C.c_char_p]
    lib.bsat_diagnostic_write.restype=C.c_int
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    manifest=json.loads((a.circuits/'manifest.json').read_text())
    items={r['file']:r for r in manifest['inputs']}
    if set(a.circuit)-items.keys():p.error('unknown circuit')
    report=dict(scope=__doc__,complete=False,platform=platform.platform(),
                library_sha256=sha(a.library),harness_sha256=sha(__file__),circuits=manifest,
                converter_sha256=sha(os.environ['BSAT_DRAT_TRIM']),
                checker_sha256=sha(os.environ['BSAT_CAKE_LPR']),cpu_limit=a.cpu,
                conflict_limit=a.conflicts,accounting=a.accounting,profile=a.profile,
                flags=3,runs=[],timing_scope='Fresh handle each query; load CPU separate from solve CPU; exports/checking excluded. Instrumented runs are not timing scores.')
    a.output.parent.mkdir(parents=True,exist_ok=True)
    def save():a.output.write_text(json.dumps(report,indent=2)+'\n')
    try:
        for name in a.circuit:
            item=items[name];source=a.circuits/item['aag'];assert sha(source)==item['aag_sha256']
            circuit=Circuit.read(source.read_text())
            clauses=[c for frame in range(a.depth+1) for c in circuit.frame(frame)]
            query=circuit.literal(circuit.output,a.depth)
            exact=clauses+[[query]];n=1+(a.depth+1)*circuit.maximum
            canonical=f'p cnf {n} {len(exact)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in exact)
            for repeat in range(a.repeats):
                for mode in ('assumption','unit') if repeat%2==0 else ('unit','assumption'):
                    s=lib.bsat_create(1,3);assert s
                    try:
                        assert lib.bsat_diagnostic_configure(s,a.profile.encode(),a.accounting)
                        start=time.process_time()
                        for clause in exact if mode=='unit' else clauses:
                            assert lib.bsat_add_clause(s,literals(clause),len(clause))
                        load_cpu=time.process_time()-start
                        assumptions=[] if mode=='unit' else [query]
                        assert lib.bsat_set_query_limits(s,a.cpu,a.conflicts,0)
                        lib.bsat_diagnostic_begin_query(s)
                        start=time.process_time();result=lib.bsat_solve(s,literals(assumptions),len(assumptions));cpu=time.process_time()-start
                        assert result in (0,10,20) and not lib.bsat_error(s)
                        stats=Stats();assert lib.bsat_get_stats(s,C.byref(stats),C.sizeof(stats))
                        size=C.c_uint64();assert lib.bsat_get_journal_bytes(s,C.byref(size))
                        row=dict(circuit=name,depth=a.depth,repeat=repeat,mode=mode,query=query,
                                 input_sha256=hashlib.sha256(canonical.encode()).hexdigest(),result=result,
                                 cpu_seconds=cpu,load_cpu_seconds=load_cpu,load_and_solve_cpu_seconds=load_cpu+cpu,
                                 within_cpu_budget=bool(result and cpu<=a.cpu),journal_bytes=size.value,
                                 stats={k:getattr(stats,k) for k,_ in stats._fields_})
                        report['runs'].append(row)
                        with tempfile.TemporaryDirectory(prefix='bsat-query-context-') as tmp:
                            root=Path(tmp);diag=root/'diagnostic.json'
                            assert lib.bsat_diagnostic_write(s,os.fsencode(diag))
                            row['diagnostic']=json.loads(diag.read_text())
                            if result:
                                cnf=root/'input.cnf';proof=root/'proof.drat'
                                start=time.process_time()
                                assert lib.bsat_export_query(s,os.fsencode(cnf),os.fsencode(proof))
                                row['export_cpu_seconds']=time.process_time()-start
                                _,exported=parse_cnf(cnf.read_text());assert exported==exact
                                if result==10:
                                    model='v '+' '.join(str(lib.bsat_value(s,v) or v) for v in range(1,n+1))+' 0\n'
                                    assert model_valid(exact,model)
                                    values={v:(lib.bsat_value(s,v) or v)>0 for v in range(1,n+1)}
                                    assert circuit.simulate(values,a.depth)[a.depth]
                                    row['verified']=True
                                else:
                                    checked=verify(cnf,proof,os.environ['BSAT_DRAT_TRIM'],os.environ['BSAT_CAKE_LPR'],root/'check',600,2048,512)
                                    row['validation']=checked;row['verified']=checked['verified'];save()
                                    assert checked['verified'],checked
                                    row['proof_sha256']=checked['drat_sha256']
                        save();print(name,mode,repeat,result,round(cpu,3),stats.conflicts,flush=True)
                    finally:lib.bsat_destroy(s)
        report['complete']=True
    except BaseException as error:
        report['error']=repr(error);raise
    finally:save()

if __name__=='__main__':main()

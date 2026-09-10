#!/usr/bin/env python3
"""Isolated per-family embedding RSS, replaying independently checked contexts."""
import argparse,ctypes as C,hashlib,json,resource,subprocess,sys,time,math
from pathlib import Path
from aag_history import Circuit
from incremental_reference import Reference
from public_api import library,literals
from validate import model_valid

def worker(a):
    manifest=json.loads((a.circuits/'manifest.json').read_text());item=next(i for i in manifest['inputs'] if i['file']==a.circuit)
    source=a.circuits/item['aag'];assert hashlib.sha256(source.read_bytes()).hexdigest()==item['aag_sha256'];circuit=Circuit.read(source.read_text())
    prior=json.loads(a.checked_report.read_text());assert prior['complete']
    truth={(r['circuit'],r['depth'],r['polarity']):r for r in prior['runs'] if r['repeat']==0}
    lib=library(a.library.resolve()) if a.solver=='bsat' else None
    handle=lib.bsat_create(1,3) if lib else Reference(a.reference.resolve(),a.cpu,a.reference_wall)
    if lib:assert handle and lib.bsat_set_query_limits(handle,a.cpu,0,0)
    clauses=[];frame=0;rows=[];start=time.monotonic()
    try:
        for depth in sorted(set(map(int,a.depths.split(',')))):
            while frame<=depth:
                for clause in circuit.frame(frame):
                    if lib:assert lib.bsat_add_clause(handle,literals(clause),len(clause))
                    else:handle.add(clause)
                    clauses.append(clause)
                frame+=1
            n=1+frame*circuit.maximum
            for polarity in (1,-1):
                assumption=polarity*circuit.literal(circuit.output,depth);exact=clauses+[[assumption]]
                data=f'p cnf {n} {len(exact)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in exact)
                digest=hashlib.sha256(data.encode()).hexdigest();expected=truth[a.circuit,depth,polarity]
                assert digest==expected['input_sha256'] and expected['validated'] and expected['result'] in (10,20)
                del data
                before=time.process_time();result=lib.bsat_solve(handle,literals([assumption]),1) if lib else handle.solve([assumption]);cpu=time.process_time()-before
                assert result in (0,10,20) and (not result or result==expected['result'])
                if lib:assert not lib.bsat_error(handle)
                if result==10:
                    model='v '+' '.join(str(lib.bsat_value(handle,v) or v) for v in range(1,n+1))+' 0\n' if lib else handle.model(n)
                    assert model_valid(exact,model)
                rows.append(dict(depth=depth,polarity=polarity,result=result,cpu_seconds=cpu,input_sha256=digest,checked_context_result=expected['result']))
    finally:
        if lib:lib.bsat_destroy(handle)
        else:handle.close()
    return dict(solver=a.solver,circuit=a.circuit,runs=rows,peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*(1 if sys.platform=='darwin' else 1024),wall_seconds=time.monotonic()-start)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for k in ('library','reference','circuits','checked-report'):p.add_argument('--'+k,type=Path,required=True)
    p.add_argument('--depths',default='0,2,4,8');p.add_argument('--cpu',type=float,default=10);p.add_argument('--reference-wall',type=float,default=90)
    p.add_argument('--output',type=Path);p.add_argument('--solver',choices=['bsat','cadical']);p.add_argument('--circuit');a=p.parse_args()
    if any(not math.isfinite(x) or x<=0 for x in (a.cpu,a.reference_wall)) or min(map(int,a.depths.split(',')))<0:p.error('invalid depth or budget')
    if a.solver:print(json.dumps(worker(a)));return
    assert a.output
    d={'complete':False,'scope':'Fresh process per solver/family: Python embedding, CNF encoding/hash/model validation and one native solver. Excludes independent proof checker, other solver and parent; not native-only RSS. Replays exact independently checked baseline contexts.','checked_report_sha256':hashlib.sha256(a.checked_report.read_bytes()).hexdigest(),'library_sha256':hashlib.sha256(a.library.read_bytes()).hexdigest(),'reference_sha256':hashlib.sha256(a.reference.read_bytes()).hexdigest(),'runs':[]}
    try:
        for row in json.loads((a.circuits/'manifest.json').read_text())['inputs']:
            for solver in ('bsat','cadical'):
                command=[sys.executable,str(Path(__file__).resolve()),'--solver',solver,'--circuit',row['file'],'--depths',a.depths,'--cpu',str(a.cpu),'--reference-wall',str(a.reference_wall)]
                for k in ('library','reference','circuits','checked_report'):command+=['--'+k.replace('_','-'),str(getattr(a,k).resolve())]
                run=subprocess.run(command,text=True,capture_output=True,timeout=300,check=True);d['runs'].append(json.loads(run.stdout));print(solver,row['file'],d['runs'][-1]['peak_rss_bytes'],flush=True)
        d['complete']=True
    finally:a.output.write_text(json.dumps(d,indent=2)+'\n')
if __name__=='__main__':main()

#!/usr/bin/env python3
"""Serial fresh-process certified DIMACS ABBA screen with complete check cost."""
import argparse, ctypes as C, hashlib, json, math, os, resource, shutil, sys, tempfile, time
from pathlib import Path
from public_api import library, literals, Stats
from validate import parse_cnf
from verified_check import verify
from process_control import run_capture

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def worker(libpath, source, cpu, wall, artifacts):
    start=time.process_time();data=source.read_bytes();n,cs=parse_cnf(data.decode())
    if n<0 or any(not 0<abs(x)<=n for c in cs for x in c):raise ValueError('invalid namespace')
    lib=library(libpath.resolve());s=lib.bsat_create(1,3);assert s
    with tempfile.TemporaryDirectory(prefix='bsat-dimacs-check-') as tmp:
        folder=Path(tmp)
        try:
            for c in cs:assert lib.bsat_add_clause(s,literals(c),len(c))
            load_cpu=time.process_time()-start
            assert lib.bsat_set_query_limits(s,cpu,0,0) and lib.bsat_set_service_limits(s,wall,0)
            before=time.process_time();began=time.monotonic();r=lib.bsat_solve(s,literals([]),0)
            solve_wall=time.monotonic()-began;solve_cpu=time.process_time()-before
            assert r in (0,10,20) and not lib.bsat_error(s)
            stats=Stats();assert lib.bsat_get_stats(s,C.byref(stats),C.sizeof(stats))
            row={'result':r,'input_sha256':hashlib.sha256(data).hexdigest(),'library_sha256':sha(libpath),'load_cpu':load_cpu,'solve_cpu':solve_cpu,'solve_wall':solve_wall,'owned_capacity_bytes':stats.owned_capacity_bytes,'conflicts':stats.conflicts,'propagations':stats.propagations,'within_budget':solve_cpu<=cpu and solve_wall<=wall,'verified':False,'export_cpu':0,'check_cpu':0}
            if r:
                cnf=folder/'input.cnf';proof=folder/'proof.drat';before=time.process_time()
                assert lib.bsat_export_query(s,os.fsencode(cnf),os.fsencode(proof))
                row['export_cpu']=time.process_time()-before
                exported_n,exported=parse_cnf(cnf.read_text());assert exported==cs and exported_n<=n
                row.update(query_sha256=sha(cnf),proof_sha256=sha(proof),proof_bytes=proof.stat().st_size)
                if r==10:
                    before=time.process_time();values={v:lib.bsat_value(s,v) for v in {abs(x) for c in cs for x in c}}
                    row['verified']=all(any(values[abs(x)]==x for x in c) for c in cs)
                    row['check_cpu']=time.process_time()-before
            row['embedding_peak_rss_bytes']=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss*(1 if sys.platform=='darwin' else 1024)
            if r==20:
                check=verify(cnf,proof,os.environ['BSAT_DRAT_TRIM'],os.environ['BSAT_CAKE_LPR'],folder/'check',600,2048,512)
                row['verification']=check;row['verified']=check['verified'];row['check_cpu']=check['total_cpu_seconds']
            if r and not row['verified']:
                shutil.copytree(folder,artifacts,dirs_exist_ok=True)
            return row
        finally:lib.bsat_destroy(s)

def summarize(rows, cpu=15):
    summary={'versions':{},'losses':[]}
    grouped={}
    for r in rows:
        assert r['version'] in ('baseline','candidate') and r['result'] in (0,10,20)
        assert not r['result'] or r['verified']
        for k in ('load_cpu','solve_cpu','export_cpu','check_cpu'):
            assert math.isfinite(r[k]) and r[k]>=0
        grouped.setdefault(r['input'],[]).append(r)
    for name,rs in grouped.items():
        assert len(rs)==4 and [r['version'] for r in rs]==['baseline','candidate','candidate','baseline']
        assert len({r['input_sha256'] for r in rs})==1
        assert len({r['result'] for r in rs if r['result']})<=1
        checked=lambda v:sum(bool(r['result'] and r['verified'] and r['within_budget']) for r in rs if r['version']==v)
        if checked('candidate')<checked('baseline'):summary['losses'].append(name)
    for v in ('baseline','candidate'):
        rs=[r for r in rows if r['version']==v]
        okay=lambda r:bool(r['result'] and r['verified'] and r['within_budget'])
        summary['versions'][v]={'queries':len(rs),'checked':sum(okay(r) for r in rs),'solve_cpu_par2_sum':sum(r['solve_cpu'] if okay(r) else 2*cpu for r in rs),'complete_cpu_par2_sum':sum(r['load_cpu']+r['solve_cpu']+r['export_cpu']+r['check_cpu'] if okay(r) else 2*cpu for r in rs),'embedding_peak_rss_bytes':max(r['embedding_peak_rss_bytes'] for r in rs)}
    b=summary['versions']['baseline'];c=summary['versions']['candidate']
    summary['gate_pass']=not summary['losses'] and c['solve_cpu_par2_sum']<=1.05*b['solve_cpu_par2_sum'] and c['complete_cpu_par2_sum']<=1.05*b['complete_cpu_par2_sum']
    return summary

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('baseline','candidate','manifest','output'):p.add_argument('--'+name,type=Path)
    p.add_argument('--worker',type=Path);p.add_argument('--input',type=Path);p.add_argument('--cpu',type=float,default=15);p.add_argument('--wall',type=float,default=20)
    a=p.parse_args();assert all(math.isfinite(x) and x>0 for x in (a.cpu,a.wall))
    if a.worker:
        print(json.dumps(worker(a.worker,a.input,a.cpu,a.wall,a.output)));return
    assert a.baseline and a.candidate and a.manifest and a.output
    manifest=json.loads(a.manifest.read_text());report={'complete':False,'manifest_sha256':sha(a.manifest),'scope':'Imported DIMACS directly into certified public API flags 3; no AIG preparation. Fresh embedding process per run; embedding peak excludes checker processes. Solve CPU budget excludes loading/export/checking; complete CPU includes them.','cpu':a.cpu,'wall':a.wall,'runs':[]}
    a.output.parent.mkdir(parents=True,exist_ok=True)
    for source,entry in manifest['inputs'].items():
        assert sha(source)==entry['sha256']
        for i,v in enumerate(('baseline','candidate','candidate','baseline')):
            cmd=[sys.executable,str(Path(__file__).resolve()),'--worker',str(getattr(a,v).resolve()),'--input',source,'--cpu',str(a.cpu),'--wall',str(a.wall),'--output',str(a.output.parent/'unverified'/f'{Path(source).stem}-{i}')]
            run=run_capture(cmd,1300);assert run.returncode==0,run.stderr
            r=json.loads(run.stdout);r.update(version=v,input=source,family=entry['family']);report['runs'].append(r)
            a.output.write_text(json.dumps(report,indent=2)+'\n')
            print(entry['family'],v,r['result'],r['verified'],flush=True)
    report['summary']=summarize(report['runs'],a.cpu);report['complete']=True;a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report['summary'],indent=2))
if __name__=='__main__':main()

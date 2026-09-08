#!/usr/bin/env python3
"""Record actual PMU availability, then collect CPU-clock stacks on Linux."""
import argparse,hashlib,json,os
from pathlib import Path
from profile_linux import parse_perf,EVENTS
from process_control import run_capture
from validate import model_valid,parse_cnf
from verified_check import verify
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--solver',required=True,type=Path);p.add_argument('--perf',required=True,type=Path)
p.add_argument('--output',required=True,type=Path)
p.add_argument('inputs',nargs='+',type=Path);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=False)
sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
cpu=min(os.sched_getaffinity(0));prefix=[]
report=dict(scope=__doc__,cpu=cpu,solver_sha256=sha(a.solver),perf=str(a.perf),runs=[],complete=False)
try:
 for index,inp in enumerate(a.inputs):
    folder=a.output/str(index);folder.mkdir();proof=folder/'proof.drat'
    base=[str(a.solver.resolve()),'--chrono','--congruence','--equiv','--equiv-budget','100000000','--alternating','--conflicts','100000','--binary-proof','--proof',str(proof.resolve()),str(inp.resolve())]
    counters=folder/'counters.csv'
    cmd=['taskset','-c',str(cpu),*prefix,str(a.perf),'stat','-x',';','--no-big-num','-o',str(counters.resolve()),'-e',','.join(EVENTS),'--',*base]
    stat=run_capture(cmd,120)
    row=dict(input=str(inp),input_sha256=sha(inp),stat_command=cmd,stat_returncode=stat.returncode,stat_stdout=stat.stdout,stat_stderr=stat.stderr)
    report['runs'].append(row)
    try:row['hardware']=parse_perf(counters.read_text())
    except (ValueError,OSError) as e:row['hardware_unavailable']=str(e)
    if 'hardware' in row:
        assert stat.returncode in (0,10,20)
        assert [line for line in stat.stdout.splitlines() if line.startswith('s ')] == [{0:'s UNKNOWN',10:'s SATISFIABLE',20:'s UNSATISFIABLE'}[stat.returncode]]
        if stat.returncode==10:
            _,clauses=parse_cnf(inp.read_text());assert model_valid(clauses,stat.stdout)
        elif stat.returncode==20:
            assert verify(inp,proof,os.environ['BSAT_DRAT_TRIM'],os.environ['BSAT_CAKE_LPR'],folder/'stat-check',60)['verified']
    data=folder/'perf.data' 
    cmd=['taskset','-c',str(cpu),*prefix,str(a.perf),'record','-e','cpu-clock:u','-F','199','--call-graph','dwarf,8192','-o',str(data.resolve()),'--',*base]
    recorded=run_capture(cmd,120)
    row.update(record_command=cmd,record_returncode=recorded.returncode,stdout=recorded.stdout,stderr=recorded.stderr)
    if recorded.returncode not in (0,10,20):
        row['sampling_unavailable']=True;continue
    assert [line for line in recorded.stdout.splitlines() if line.startswith('s ')] == [{0:'s UNKNOWN',10:'s SATISFIABLE',20:'s UNSATISFIABLE'}[recorded.returncode]]
    if recorded.returncode==10:
        _,clauses=parse_cnf(inp.read_text());assert model_valid(clauses,recorded.stdout)
    elif recorded.returncode==20:
        assert verify(inp,proof,os.environ['BSAT_DRAT_TRIM'],os.environ['BSAT_CAKE_LPR'],folder/'check',60)['verified']
    row['result']=recorded.returncode;row['conclusive_verified']=recorded.returncode in (10,20)
    text=run_capture([*prefix,str(a.perf),'report','--stdio','--no-children','--sort','symbol','-i',str(data.resolve())],60)
    (folder/'stacks.txt').write_text(text.stdout);row['report_returncode']=text.returncode
    row['perf_data_sha256']=sha(data);row['proof_sha256']=sha(proof)
    assert text.returncode==0
 report['complete']=True
finally:(a.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')

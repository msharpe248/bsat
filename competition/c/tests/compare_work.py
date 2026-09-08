#!/usr/bin/env python3
"""Compare process cost at bounded search work, with strict trace/byte guards.

CPU per propagation includes loading and proof output: it is not isolated BCP time.
UNKNOWN proof prefixes are trace evidence, never UNSAT certificates.
"""
import argparse
import hashlib
import json
from pathlib import Path
import random
import resource
import shlex
import subprocess
import tempfile
import time
from validate import checker_verified, model_valid, parse_cnf

COUNTERS=('Decisions','Propagations','Conflicts','Restarts','Learned clauses',
          'Learned literals','Deleted clauses','Minimized literals','Max LBD',
          'Garbage collections','Reductions','Substituted vars','Literal inspections')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--solver',action='append',required=True,help='NAME=EXECUTABLE')
    p.add_argument('--options',default='');p.add_argument('--conflicts',type=int,default=20000)
    p.add_argument('--formats',default='binary,text,none');p.add_argument('--repeats',type=int,default=3)
    p.add_argument('--seed',type=int,default=20261326);p.add_argument('--checker',required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('inputs',nargs='+',type=Path)
    args=p.parse_args();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    solvers={name:str(Path(path).resolve()) for name,path in (s.split('=',1) for s in args.solver)}
    assert len(solvers)==len(args.solver) and args.conflicts>0 and args.repeats>0
    formats=args.formats.split(',');assert set(formats)<= {'binary','text','none'}
    inputs=[x.resolve() for x in args.inputs]
    report={'scope':__doc__,'seed':args.seed,'conflict_limit':args.conflicts,'repeats':args.repeats,
            'options':shlex.split(args.options),'solvers':{k:{'path':v,'sha256':sha(Path(v))} for k,v in solvers.items()},
            'inputs':{str(i):sha(i) for i in inputs},'checker_sha256':sha(Path(args.checker)),'runs':[]}
    # Freeze policy before starting any timed process.
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.with_suffix('.policy.json').write_text(json.dumps(report,indent=2)+'\n')
    jobs=[(s,i,f,r) for s in solvers for i in inputs for f in formats for r in range(args.repeats)]
    random.Random(args.seed).shuffle(jobs);traces={}
    with tempfile.TemporaryDirectory(prefix='bsat-work-') as tmp:
        proof=Path(tmp)/'proof'
        for name,inp,fmt,repeat in jobs:
            cmd=[solvers[name],*report['options'],'--conflicts',str(args.conflicts)]
            if fmt!='none':cmd+=['--proof',str(proof)]
            if fmt=='binary':cmd+=['--binary-proof']
            cmd+=[str(inp)];before=resource.getrusage(resource.RUSAGE_CHILDREN);start=time.monotonic()
            run=subprocess.run(cmd,capture_output=True,text=True,timeout=120)
            wall=time.monotonic()-start;after=resource.getrusage(resource.RUSAGE_CHILDREN)
            cpu=after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime
            stats={}
            for line in run.stdout.splitlines():
                if line.startswith('c ') and ':' in line:
                    k,v=line[2:].split(':',1);stats[k.strip()]=v.strip()
            row={'solver':name,'input':str(inp),'format':fmt,'repeat':repeat,'command':cmd,
                 'returncode':run.returncode,'cpu_seconds':cpu,'wall_seconds':wall,'stats':stats,
                 'stdout':run.stdout,'stderr':run.stderr,'proof_sha256':sha(proof) if fmt!='none' else None}
            report['runs'].append(row);args.output.write_text(json.dumps(report,indent=2)+'\n')
            assert run.returncode in (0,10,20),row
            counters={k:int(stats[k]) for k in COUNTERS}
            row['process_cpu_per_million_propagations']=cpu*1e6/counters['Propagations'] if counters['Propagations'] else None
            trace=(run.returncode,counters,row['proof_sha256'])
            key=(str(inp),fmt)
            if key in traces:assert traces[key]==trace,('search/proof changed',key,row)
            else:traces[key]=trace
            row['verified']=False
            if run.returncode==10:
                _,clauses=parse_cnf(inp.read_text());assert model_valid(clauses,run.stdout);row['verified']=True
            elif run.returncode==20 and fmt!='none':
                checked=subprocess.run([args.checker,str(inp),str(proof)],capture_output=True,text=True,timeout=600)
                assert checker_verified(checked),(checked.stdout,checked.stderr);row['verified']=True
            print(f'{name} {inp.parent.name} {fmt}: {cpu:.6f}s CPU, {counters["Conflicts"]} conflicts',flush=True)
    report['complete']=True;report['identical_traces']=True
    args.output.write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':main()

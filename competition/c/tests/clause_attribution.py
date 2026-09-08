#!/usr/bin/env python3
"""Intrusive fixed-conflict attribution, preserving all hot live clause rows."""
import argparse, hashlib, json, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--solver',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--conflicts',type=int,default=100000)
p.add_argument('inputs',nargs='+',type=Path)
a=p.parse_args()
assert a.conflicts>0
sha=lambda f:hashlib.sha256(f.read_bytes()).hexdigest()
report=dict(scope=__doc__,solver_sha256=sha(a.solver),conflicts=a.conflicts,runs=[])
for inp in a.inputs:
    cmd=[str(a.solver.resolve()),'--accounting','--chrono','--congruence','--equiv','--equiv-budget','100000000','--alternating','--conflicts',str(a.conflicts),str(inp.resolve())]
    r=subprocess.run(cmd,capture_output=True,text=True,timeout=120)
    assert r.returncode in (0,10,20),(r.stdout,r.stderr)
    counters={};hot=[]
    for line in r.stdout.splitlines():
        if line.startswith('c Search ') and ':' in line:
            k,v=line[9:].split(':',1)
            if v.strip().isdigit():counters[k]=int(v)
        if line.startswith('c Hot live clause:'):
            hot.append({k:int(v) for k,v in (pair.split('=') for pair in line.split(':',1)[1].split())})
    assert counters['replacement_scans']>0 and hot
    assert counters['replacement_scans']==counters['original_scans']+counters['learned_scans']
    report['runs'].append(dict(input=str(inp.resolve()),input_sha256=sha(inp),command=cmd,result=r.returncode,counters=counters,hot_live=hot,stdout=r.stdout,stderr=r.stderr))
    a.output.write_text(json.dumps(report,indent=2)+'\n')
    print(inp.name,counters['original_scans'],counters['learned_scans'],flush=True)

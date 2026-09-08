#!/usr/bin/env python3
"""Compare deterministic fixed-conflict traces; UNKNOWN prefix equality is not a proof."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import time
import random


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--before',required=True,type=Path);p.add_argument('--after',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path);p.add_argument('inputs',nargs='+',type=Path)
    p.add_argument('--conflicts',type=int,default=1000);p.add_argument('--repeats',type=int,default=1)
    a=p.parse_args()
    if a.conflicts<1 or a.repeats<1:p.error('positive conflicts and repeats required')
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();rows=[]
    keys=('Conflicts','Decisions','Propagations','Learned clauses','Restarts')
    with tempfile.TemporaryDirectory(prefix='bsat-traces-') as temp:
        proof=Path(temp)/'proof'
        jobs=[(inp,repeat) for inp in a.inputs for repeat in range(a.repeats)]
        random.Random(2026090833).shuffle(jobs)
        for inp,repeat in jobs:
            pair=[]
            binaries=[a.before,a.after] if repeat%2==0 else [a.after,a.before]
            for binary in binaries:
                cmd=[str(binary.resolve()),'--chrono','--congruence','--equiv','--equiv-budget','100000000',
                     '--alternating','--conflicts',str(a.conflicts),'--binary-proof','--proof',str(proof),str(inp.resolve())]
                start=time.perf_counter();r=subprocess.run(cmd,capture_output=True,text=True,timeout=60)
                seconds=time.perf_counter()-start
                assert r.returncode in (0,10,20),(cmd,r.stdout,r.stderr)
                stats={}
                for line in (r.stdout+'\n'+r.stderr).splitlines():
                    if line.startswith('c ') and ':' in line:
                        k,v=line[2:].split(':',1)
                        if k.strip() in keys:stats[k.strip()]=v.strip()
                assert all(k in stats for k in ('Conflicts','Decisions','Propagations'))
                pair.append(dict(binary=str(binary),seconds=seconds,binary_sha256=sha(binary),result=r.returncode,stats=stats,proof_sha256=sha(proof)))
            assert {k:v for k,v in pair[0].items() if k not in ('binary_sha256','seconds','binary')}=={k:v for k,v in pair[1].items() if k not in ('binary_sha256','seconds','binary')},pair
            rows.append(dict(input=str(inp),repeat=repeat,input_sha256=sha(inp),traces=pair,equal=True))
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(dict(scope=__doc__,conflicts=a.conflicts,repeats=a.repeats,seed=2026090833,runs=rows),indent=2)+'\n')
    print('PASS:',len(rows),'identical deterministic search traces')


if __name__=='__main__':main()

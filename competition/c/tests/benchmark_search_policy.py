#!/usr/bin/env python3
"""Serial phase or propagation ablations with a frozen policy and exact input hashes."""
import argparse
import json
import math
from pathlib import Path
import shlex
import subprocess
import sys
from benchmark import digest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--solver',required=True,type=Path)
    p.add_argument('--checker',required=True)
    p.add_argument('--output',required=True,type=Path)
    p.add_argument('--suite',choices=['phase','propagation'],default='phase')
    p.add_argument('--accounting',action='store_true')
    p.add_argument('--seconds',type=float,default=12)
    p.add_argument('--repeats',type=int,default=2)
    p.add_argument('inputs',nargs='+',type=Path)
    a=p.parse_args()
    if not math.isfinite(a.seconds) or a.seconds<=0 or a.repeats<1:p.error('positive finite seconds and repeats required')
    base=[str(a.solver.resolve()),'--chrono','--congruence','--equiv','--equiv-budget','100000000','--alternating']
    profiles={'control':[],'no-rephase':['--no-rephase'],'refresh':['--rephase-refresh'],'diversify':['--rephase-diversify']}
    if a.suite=='propagation':
        profiles={'control':[],'linear-watch':['--no-circular'],'dynamic-lbd':['--dynamic-lbd'],'protect-used':['--protect-used']}
    suffix=['--binary-proof','--proof','{proof}','{input}']
    # Native bounded diagnostic runs exit cleanly and emit counters. Intrusive
    # CPU accounting has its own clock policy and is not used for speed scores.
    if a.accounting:suffix=['--accounting','--time',str(a.seconds)]+suffix
    commands={k:base+v+suffix for k,v in profiles.items()}
    policy=dict(scope=__doc__,suite=a.suite,accounting=a.accounting,commands=commands,
                solver_sha256=digest(a.solver),inputs={str(f.resolve()):{'sha256':digest(f)} for f in a.inputs},
                seconds=a.seconds,repeats=a.repeats,seed=2026090832)
    a.output.parent.mkdir(parents=True,exist_ok=True)
    manifest=a.output.with_suffix('.policy.json');manifest.write_text(json.dumps(policy,indent=2)+'\n')
    cmd=[sys.executable,str(Path(__file__).with_name('benchmark.py')),'--manifest',str(manifest),
         '--checker',a.checker,'--check-timeout','600','--timeout',str(a.seconds+5 if a.accounting else a.seconds),
         '--repeats',str(a.repeats),'--seed',str(policy['seed']),'--output',str(a.output)]
    if not a.accounting:cmd+=['--external-wall-only']
    for k,v in commands.items():cmd+=['--solver',k+'='+shlex.join(v)]
    subprocess.run(cmd,check=True)


if __name__=='__main__':main()

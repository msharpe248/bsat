#!/usr/bin/env python3
"""Frozen cal3 learning-policy ablations; no default promotion evidence."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import sys
from benchmark import digest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('bsat','cadical','input','output'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--conflicts',type=int,default=0)
    a=p.parse_args()
    if a.conflicts<0:p.error('conflicts must be nonnegative')
    bsat={'default':[], 'no-minimize':['--no-minimize'],
          'iterative':['--iterative-minimize'], 'binary':['--binary-minimize'],
          'combined':['--iterative-minimize','--binary-minimize'],
          'retain-more':['--reduce-fraction','0.9'],
          'no-reduction':['--reduce-interval','1000000000']}
    cadical={'default':[], 'plain':['--plain'],
             'plain-no-minimize':['--plain','--no-minimize'],
             'plain-no-shrink':['--plain','--shrink=0'],
             'plain-no-minimize-no-shrink':['--plain','--no-minimize','--shrink=0'],
             'plain-no-reduction':['--plain','--no-reduce']}
    suffix=['--binary-proof','--proof','{proof}','{input}']
    if a.conflicts:suffix=['--conflicts',str(a.conflicts),*suffix]
    commands={'bsat-'+n:[str(a.bsat.resolve()),*f,*suffix] for n,f in bsat.items()}
    for n,f in cadical.items():
        commands['cadical-'+n]=[str(a.cadical.resolve()),*f,
            *(['-c',str(a.conflicts)] if a.conflicts else []),'{input}','{proof}']
    policy=dict(scope=__doc__,cpu_seconds=10,wall_seconds=15,repeats=2,seed=2026090869,
                fixed_conflicts=a.conflicts,commands=commands,
                cadical_revision='c60730422e758ef1cebe7aeddf2dda31c996bf04',
                boundary='Permanent-unit query snapshot; development ablations with different search trajectories',
                binaries={str(f.resolve()):digest(f) for f in (a.bsat,a.cadical)},
                inputs={str(a.input.resolve()):dict(sha256=digest(a.input))})
    manifest=a.output.with_suffix('.policy.json');manifest.write_text(json.dumps(policy,indent=2)+'\n')
    cmd=[sys.executable,str(Path(__file__).with_name('benchmark.py')),'--checker',
         str(Path(__file__).with_name('verified_check.py')),'--check-timeout','120',
         '--cpu-limit','10','--timeout','15','--repeats','2','--seed','2026090869',
         '--manifest',str(manifest),'--output',str(a.output)]
    if not a.conflicts:cmd.append('--external-wall-only')
    for name,command in commands.items():cmd+=['--solver',name+'='+shlex.join(command)]
    subprocess.run(cmd,check=True)


if __name__=='__main__':main()

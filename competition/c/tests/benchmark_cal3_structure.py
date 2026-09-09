#!/usr/bin/env python3
"""Frozen one-shot structural ablations of an exact cal3 query snapshot."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess
import sys
from benchmark import digest

p=argparse.ArgumentParser(description=__doc__)
for name in ('bsat','cadical','kissat','input','output'):
    p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args()
suffix=['--binary-proof','--proof','{proof}','{input}']
profiles={'default':[], 'equiv':['--equiv','--equiv-budget','100000000'],
          'congruence':['--congruence','--congruence-budget','100000000'],
          'combined':['--congruence','--congruence-budget','100000000','--equiv','--equiv-budget','100000000'],
          'eliminate':['--elim']}
commands={'bsat-'+name:[str(a.bsat.resolve()),*flags,*suffix] for name,flags in profiles.items()}
commands.update({'cadical':[str(a.cadical.resolve()),'{input}','{proof}'],
                 'cadical-plain':[str(a.cadical.resolve()),'--plain','{input}','{proof}'],
                 'kissat':[str(a.kissat.resolve()),'-s','{input}','{proof}']})
policy=dict(scope=__doc__,cpu_seconds=5,wall_seconds=10,repeats=2,seed=2026090864,
            boundary='Snapshot includes the query assumption as a permanent unit; not retained API semantics',
            commands=commands,binaries={str(f.resolve()):digest(f) for f in (a.bsat,a.cadical,a.kissat)},
            inputs={str(a.input.resolve()):dict(sha256=digest(a.input))})
manifest=a.output.with_suffix('.policy.json');manifest.write_text(json.dumps(policy,indent=2)+'\n')
cmd=[sys.executable,str(Path(__file__).with_name('benchmark.py')),'--checker',
     str(Path(__file__).with_name('verified_check.py')),'--check-timeout','120',
     '--external-wall-only','--cpu-limit','5','--timeout','10','--repeats','2',
     '--seed','2026090864','--manifest',str(manifest),'--output',str(a.output)]
for name,command in commands.items():cmd+=['--solver',name+'='+shlex.join(command)]
subprocess.run(cmd,check=True)

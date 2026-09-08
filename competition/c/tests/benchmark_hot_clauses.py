#!/usr/bin/env python3
"""Frozen development screen for the archived bounded hot-clause prototype."""
import argparse,json,shlex,subprocess,sys
from pathlib import Path
from benchmark import digest
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--before',required=True,type=Path);p.add_argument('--candidate',required=True,type=Path)
p.add_argument('--output',required=True,type=Path);p.add_argument('inputs',nargs='+',type=Path)
a=p.parse_args()
base=['--chrono','--congruence','--equiv','--equiv-budget','100000000','--alternating']
suffix=['--binary-proof','--proof','{proof}','{input}']
commands={'baseline':[str(a.before.resolve())]+base+suffix,
          'broad':[str(a.before.resolve())]+base+['--inprocess']+suffix,
          'hot':[str(a.candidate.resolve())]+base+['--hot-vivify']+suffix}
policy=dict(scope=__doc__,seconds=8,repeats=2,seed=2026090842,commands=commands,
            binaries={str(f):digest(f) for f in (a.before,a.candidate)},
            inputs={str(f.resolve()):dict(sha256=digest(f)) for f in a.inputs})
manifest=a.output.with_suffix('.policy.json');manifest.write_text(json.dumps(policy,indent=2)+'\n')
cmd=[sys.executable,str(Path(__file__).with_name('benchmark.py')),'--checker',str(Path(__file__).with_name('verified_check.py')),
     '--external-wall-only','--timeout','8','--check-timeout','600','--repeats','2','--seed','2026090842',
     '--manifest',str(manifest),'--output',str(a.output)]
for k,v in commands.items():cmd+=['--solver',k+'='+shlex.join(v)]
subprocess.run(cmd,check=True)

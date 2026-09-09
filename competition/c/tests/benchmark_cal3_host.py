#!/usr/bin/env python3
"""Matched cal3 host comparison and narrowly scoped reference controls."""
import argparse,gzip,json,subprocess,sys,shlex
from pathlib import Path
from benchmark import digest
p=argparse.ArgumentParser(description=__doc__)
for n in ('bsat','cadical','kissat','output'):p.add_argument('--'+n,type=Path,required=True)
p.add_argument('--controls',action='store_true')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
fixture=Path(__file__).parent/'fixtures/acceptance/cal3-query.cnf.gz'
cnf=a.output/'cal3.cnf';cnf.write_bytes(gzip.decompress(fixture.read_bytes()))
profiles={'bsat':[str(a.bsat.resolve()),'--binary-proof','--proof','{proof}','{input}'],
          'cadical':[str(a.cadical.resolve()),'{input}','{proof}'],
          'kissat':[str(a.kissat.resolve()),'-s','{input}','{proof}']}
if a.controls:
 profiles={n:[str(a.cadical.resolve()),'--plain',*f,'{input}','{proof}'] for n,f in {
  'plain':[], 'no-otfs':['--no-otfs'], 'no-bumpreason':['--no-bumpreason']}.items()}
policy={'inputs':{str(cnf.resolve()):{'sha256':digest(cnf)}},'cpu_seconds':60,'wall_seconds':90,'repeats':2,'seed':2026090901,'commands':profiles}
manifest=a.output/'policy.json';manifest.write_text(json.dumps(policy,indent=2)+'\n')
cmd=[sys.executable,str(Path(__file__).with_name('benchmark.py')),'--checker',str(Path(__file__).with_name('verified_check.py')),'--cpu-limit','60','--timeout','90','--check-timeout','600','--repeats','2','--seed','2026090901','--external-wall-only','--manifest',str(manifest),'--output',str(a.output/'results.json')]
for n,c in profiles.items():cmd+=['--solver',n+'='+shlex.join(c)]
subprocess.run(cmd,check=True)

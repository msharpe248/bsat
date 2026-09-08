#!/usr/bin/env python3
"""Run generated BMC/configuration histories with exact domain oracles."""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import random
import resource
import subprocess
import time

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--driver',type=Path,required=True);p.add_argument('--repeats',type=int,default=2);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    driver=a.driver.resolve();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    libraries=list(driver.parent.glob('libbsat_*'))
    d={'scope':__doc__,'driver_sha256':sha(driver),'libraries':{str(p):sha(p) for p in libraries},'repeats':a.repeats,'seed':2026090825,'query_limits':{'cpu_seconds':0.2,'conflicts':2000},'recovery_query_limits':{'kind':'bmc','query':33,'cpu_seconds':0,'conflicts':1000000},'runs':[]}
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.with_suffix('.policy.json').write_text(json.dumps(d,indent=2)+'\n')
    jobs=list(itertools.product(['bmc','configuration'],[0,1],range(a.repeats)));random.Random(d['seed']).shuffle(jobs)
    for kind,reuse,repeat in jobs:
        start=time.monotonic();before=resource.getrusage(resource.RUSAGE_CHILDREN)
        r=subprocess.run([str(driver),kind,str(reuse)],capture_output=True,text=True,timeout=240)
        after=resource.getrusage(resource.RUSAGE_CHILDREN);wall=time.monotonic()-start
        assert r.returncode==0,(r.stdout,r.stderr)
        rows=[json.loads(line) for line in r.stdout.splitlines()]
        assert len(rows)==(66 if kind=='bmc' else 512)
        assert max(row['vars'] for row in rows)>=4096
        assert all(row['result'] in (0,row['oracle']) for row in rows)
        for q in rows:
            recovery=kind=='bmc' and q['query']==33
            assert q['cpu_limit']==(0 if recovery else 0.2)
            assert q['conflict_limit']==(1000000 if recovery else 2000)
            if recovery: assert q['result']==10
        row={'kind':kind,'reuse':reuse,'repeat':repeat,'wall_seconds':wall,'process_cpu_seconds':after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime,'queries':rows}
        d['runs'].append(row);a.output.write_text(json.dumps(d,indent=2)+'\n')
        print(kind,reuse,repeat,'queries',len(rows),'unknown',sum(x['result']==0 for x in rows),f'{wall:.3f}s',flush=True)
    d['complete']=True;a.output.write_text(json.dumps(d,indent=2)+'\n')
if __name__=='__main__':main()

#!/usr/bin/env python3
"""Evaluate the predeclared probing target and confirmation acceptance gates."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--reports',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();sources={}
def read(name):
    file=a.reports/name;data=json.loads(file.read_text());assert data['complete'] and not data['accounting']
    assert all(r['validated'] for r in data['runs'])
    sources[name]=hashlib.sha256(file.read_bytes()).hexdigest();return data
report=dict(complete=False,target={})
for flags in (2,3):
    modes={}
    for profile in ('control','probe-default-budget'):
        values=[]
        for repeat in range(3):
            data=read(f'gen23-probe-comparison-{profile}-{repeat}-20260908.json')
            rows=[r for r in data['runs'] if r['flags']==flags]
            assert len(rows)==12 and all(r['within_cpu_budget'] for r in rows)
            values.append(dict(cpu=sum(r['cpu_seconds'] for r in rows),conflicts=sum(r['diagnostic']['conflicts'] for r in rows),journal_peak=max(r['journal_bytes'] for r in rows)))
        modes[profile]=values
    baseline=statistics.median(r['cpu'] for r in modes['control']);candidate=statistics.median(r['cpu'] for r in modes['probe-default-budget'])
    gains=[1-y['cpu']/x['cpu'] for x,y in zip(modes['control'],modes['probe-default-budget'])]
    report['target'][flags]=dict(repetitions=modes,median_gain=1-candidate/baseline,each_pair_gain=gains,passes=all(g>=.10 for g in gains))
data=[read(f'circuit-probe-confirmation-{profile}-20260908.json') for profile in ('control','probe-default-budget')]
key=lambda r:(r['circuit'],r['flags'],r['depth'],r['polarity'])
maps=[{key(r):r for r in d['runs']} for d in data];assert maps[0].keys()==maps[1].keys()
lost=[]
for k,left in maps[0].items():
    right=maps[1][k]
    assert left['input_sha256']==right['input_sha256'] and left['assumptions']==right['assumptions']
    assert not left['result'] or not right['result'] or left['result']==right['result']
    if left['within_cpu_budget'] and not right['within_cpu_budget']:lost.append(k)
totals=[sum(r['cpu_seconds'] if r['within_cpu_budget'] else 2*d['limits']['bsat_cpu'] for r in d['runs']) for d in data]
regression=totals[1]/totals[0]-1
report['confirmation']=dict(queries=len(maps[0]),lost_solves=lost,total_cpu_par2=totals,regression=regression,passes=not lost and regression<=.05)
report['sources']=sources;report['complete']=True
report['passes']=all(r['passes'] for r in report['target'].values()) and report['confirmation']['passes']
a.output.write_text(json.dumps(report,indent=2)+'\n');assert report['passes']
print('PASS: all target pairs improve >=10%, no confirmation solved-case loss, aggregate PAR-2 regression <=5%')

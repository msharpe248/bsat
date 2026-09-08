#!/usr/bin/env python3
"""Require identical query/search traces before summarizing journal-on/off timing."""
import argparse
import hashlib
import json
from pathlib import Path
import statistics


def compare(left,right):
    assert left['complete'] and right['complete']
    assert not left['accounting'] and not right['accounting']
    assert left['diagnostic_profile']=='control' and right['diagnostic_profile']=='journal-off'
    assert left['library_sha256']==right['library_sha256'] and left['limits']==right['limits']
    key=lambda r:(r['circuit'],r['flags'],r['depth'],r['polarity'],r['repeat'])
    a={key(r):r for r in left['runs']};b={key(r):r for r in right['runs']}
    assert len(a)==len(left['runs']) and len(b)==len(right['runs']) and a.keys()==b.keys()
    for k,x in a.items():
        y=b[k]
        assert x['validated'] and y['validated'] and x['within_cpu_budget'] and y['within_cpu_budget']
        assert x['result'] in (10,20) and x['result']==y['result']
        assert x['input_sha256']==y['input_sha256'] and x['assumptions']==y['assumptions']
        assert x['diagnostic']==y['diagnostic'],(k,'search work differs')
        for row in (x,y):
            assert row['bsat_model_and_simulation'] if row['result']==10 else row['fresh_proof_sha256']
    return a,b


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--pair',nargs=2,action='append',required=True,type=Path,metavar=('ON','OFF'))
    p.add_argument('--output',required=True,type=Path);args=p.parse_args()
    report=dict(complete=False,pairs=[],modes={})
    totals={}
    for on,off in args.pair:
        left,right=compare(json.loads(on.read_text()),json.loads(off.read_text()))
        report['pairs'].append(dict(on=str(on),off=str(off),on_sha256=hashlib.sha256(on.read_bytes()).hexdigest(),off_sha256=hashlib.sha256(off.read_bytes()).hexdigest(),matched_queries=len(left)))
        for flags in sorted({k[1] for k in left}):
            rows=[r for k,r in left.items() if k[1]==flags];other=[r for k,r in right.items() if k[1]==flags]
            totals.setdefault(flags,[]).append(dict(on=sum(r['cpu_seconds'] for r in rows),off=sum(r['cpu_seconds'] for r in other),conflicts=sum(r['diagnostic']['conflicts'] for r in rows),learned_literals=sum(r['diagnostic']['learned_literals'] for r in rows),peak_journal_bytes=max(r['journal_bytes'] for r in rows)))
    for flags,rows in totals.items():
        on=statistics.median(r['on'] for r in rows);off=statistics.median(r['off'] for r in rows)
        report['modes'][flags]=dict(repetitions=rows,median_on_cpu=on,median_off_cpu=off,removed_fraction=(on-off)/on)
    report['complete']=True;args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report['modes'],indent=2))


if __name__=='__main__':main()

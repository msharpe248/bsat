#!/usr/bin/env python3
"""Summarize a complete fresh circuit comparison without crediting late answers."""
import argparse,json,math,statistics
from pathlib import Path

def summarize(d):
    assert d['complete'];rows=d['runs'];assert rows
    keys=[(r['circuit'],r['repeat'],r['flags'],r['depth'],r['polarity']) for r in rows]
    assert len(set(keys))==len(keys)
    expected={(i['file'],rep,3,depth,polarity) for i in d['circuits']['inputs'] for rep in (0,1) for depth in (0,2,4,8) for polarity in (1,-1)}
    assert set(keys)==expected,'missing or extra frozen queries'
    ids={};answers={}
    for r in rows:
        assert r['validated'];key=(r['circuit'],r['depth'],r['polarity'])
        ids.setdefault(key,set()).add(r['input_sha256'])
        for k in ('result','incremental_result'):
            assert r[k] in (0,10,20)
            if r[k]:answers.setdefault(key,set()).add(r[k])
        if 20 in (r['result'],r['incremental_result']):
            assert any(c['verified'] for c in r.get('certificate_checks',{}).values())
        if r['result']==10:assert r['bsat_model_and_simulation']
        if r['incremental_result']==10:assert r['incremental_model_and_simulation']
    assert all(len(v)==1 for v in ids.values()) and all(len(v)==1 for v in answers.values())
    out={'queries':len(rows),'solvers':{},'max_bsat_owned_capacity_bytes':max(r['stats']['owned_capacity_bytes'] for r in rows),'max_journal_bytes':max(r.get('journal_bytes',0) for r in rows),'rss_scope':'Per-solver RSS is not measured by this in-process comparison; use isolated transaction acceptance for aggregate peak memory.'}
    for name,result,cpu,limit in [('bsat','result','cpu_seconds','bsat_cpu'),('cadical','incremental_result','incremental_cpu_seconds','cadical_thread_cpu')]:
        budget=d['limits'][limit];assert math.isfinite(budget) and budget>0
        assert all(math.isfinite(r[cpu]) and r[cpu]>=0 for r in rows)
        solved=[r for r in rows if r[result] and r[cpu]<=budget]
        out['solvers'][name]={'checked_within_budget':len(solved),'mean_cpu_par2':statistics.mean(r[cpu] if r in solved else 2*budget for r in rows),'unfinished_or_late':[(r['circuit'],r['repeat'],r['depth'],r['polarity']) for r in rows if r not in solved]}
    out['total_query_and_validation_wall_seconds']=sum(r['query_validation_wall_seconds'] for r in rows)
    out['cost_scope']='Includes both retained solvers, fresh validation, export and checking; excludes encoding/additions. Not a BSAT-only transaction latency.'
    return out

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('report',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    out=summarize(json.loads(a.report.read_text()));a.output.write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
if __name__=='__main__':main()

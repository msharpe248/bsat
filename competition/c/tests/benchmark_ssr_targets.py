#!/usr/bin/env python3
"""Frozen paired certified SSR target and broader validation screen."""
import argparse,json,subprocess,sys,math
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for name in ('baseline','candidate','reference','incremental-reference','original','expanded','output'):p.add_argument('--'+name,required=True,type=Path)
p.add_argument('--summarize-only',action='store_true')
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
policy={'baseline_revision':'c6b4ac84c94e231983886828d3100495a49545c6','order':['baseline','candidate','candidate','baseline'],'flags':3,'query_cpu':60,'reference_wall':90,'checker_wall':600,'checker_heap_mb':2048,'checker_stack_mb':512,'targets':{'cal3':{'circuit':'cal3.aig','depths':'0,1,2,4,8,16','suite':'original'},'cal100':{'circuit':'cal100.aig','depths':'0,2,4','suite':'expanded'}}}
(a.output/'policy.json').write_text(json.dumps(policy,indent=2)+'\n')
if not a.summarize_only:
    for target,setting in policy['targets'].items():
        for index,version in enumerate(policy['order']):
            command=[sys.executable,str(Path(__file__).with_name('industrial_histories.py')),'--library',str(getattr(a,version)),'--profile','control','--reference',str(a.reference),'--incremental-reference',str(a.incremental_reference),'--circuits',str(getattr(a,setting['suite'])),'--circuit',setting['circuit'],'--depths',setting['depths'],'--flags','3','--cpu','60','--reference-cpu','60','--reference-wall','90','--fresh-wall','90','--checker-wall','600','--checker-heap-mb','2048','--checker-stack-mb','512','--retain-unverified',str(a.output/'unverified'),'--output',str(a.output/f'{target}-{index}-{version}.json')]
            subprocess.run(command,check=True)
summary={'complete':True,'versions':{},'losses':[],'targets':{}}
for target in policy['targets']:
    by={'baseline':[],'candidate':[]};identities={};cases={}
    for index,version in enumerate(policy['order']):
        d=json.loads((a.output/f'{target}-{index}-{version}.json').read_text());assert d['complete']
        expected={(int(depth),polarity) for depth in policy['targets'][target]['depths'].split(',') for polarity in (1,-1)}
        assert len(d['runs'])==len(expected) and {(r['depth'],r['polarity']) for r in d['runs']}==expected
        for r in d['runs']:
            assert r['result'] in (0,10,20) and r['flags']==3 and r['circuit']==policy['targets'][target]['circuit']
            assert r['validated'] and math.isfinite(r['cpu_seconds']) and r['cpu_seconds']>=0
            key=(r['depth'],r['polarity']);identities.setdefault(key,set()).add(r['input_sha256'])
            case=cases.setdefault(key,{'baseline':[],'candidate':[]});case[version].append(r)
            check=r.get('certificate_checks',{}).get('bsat-check',{})
            if r['result']==20:assert check['verified'] and r['bsat_proof_sha256']
            if r['result']==10:assert r['bsat_model_and_simulation']
            checked=bool(r['result'] and r['within_cpu_budget'] and r['cpu_seconds']<=60)
            check_cpu=check.get('total_cpu_seconds',sum(x['cpu_seconds'] for x in check.get('stages',[])))
            by[version].append({'checked':checked,'solve_cpu':r['cpu_seconds'],'complete_cpu':r['cpu_seconds']+r.get('export_cpu_seconds',0)+check_cpu if checked else 120,'check_wall':check.get('seconds',0),'journal_bytes':r.get('journal_bytes',0),'owned_capacity':r['stats']['owned_capacity_bytes'],'ssr_strengthened':r['diagnostic'].get('ssr_strengthened',0)})
    assert all(len(v)==1 for v in identities.values())
    for key,case in cases.items():
        assert len({r['result'] for rows in case.values() for r in rows if r['result']})<=1
        if sum(bool(r['result'] and r['within_cpu_budget']) for r in case['candidate'])<sum(bool(r['result'] and r['within_cpu_budget']) for r in case['baseline']):summary['losses'].append([target,*key])
    summary['targets'][target]={v:{'queries':len(rows),'checked':sum(r['checked'] for r in rows),'solve_cpu_sum':sum(r['solve_cpu'] for r in rows),'complete_cpu_par2_sum':sum(r['complete_cpu'] for r in rows),'check_wall_sum':sum(r['check_wall'] for r in rows),'max_journal_bytes':max(r['journal_bytes'] for r in rows),'max_owned_capacity':max(r['owned_capacity'] for r in rows),'max_cumulative_ssr_strengthened':max(r['ssr_strengthened'] for r in rows)} for v,rows in by.items()}
for v in ('baseline','candidate'):
    summary['versions'][v]={'complete_cpu_par2_sum':sum(t[v]['complete_cpu_par2_sum'] for t in summary['targets'].values())}
ratio=summary['versions']['candidate']['complete_cpu_par2_sum']/summary['versions']['baseline']['complete_cpu_par2_sum']
summary['relative_complete_cpu_change']=ratio-1;summary['gate_pass']=not summary['losses'] and ratio<=1.05
summary['benefit_demonstrated']=not summary['losses'] and ratio<.95
(a.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))

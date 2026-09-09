#!/usr/bin/env python3
"""Summarize checked query outcomes from the frozen incremental host comparison."""
import argparse
import json
import math
from pathlib import Path
import statistics

p=argparse.ArgumentParser(description=__doc__);p.add_argument('directory',type=Path);a=p.parse_args()
policy=json.loads((a.directory/'policy.json').read_text())
summary={'scope':__doc__,'policy':policy,'suites':{},'gate_pass':True}
for suite in ('original','expanded'):
    by_version={'baseline':[],'candidate':[]};identities={};cases={}
    for index,version in enumerate(policy['order']):
        report=json.loads((a.directory/f'{suite}-{index}-{version}.json').read_text())
        assert report['complete'],(suite,index,'incomplete')
        assert report['limits']['bsat_cpu']==policy[suite]['cpu']
        expected={(item['file'],policy['flags'],depth,polarity)
                  for item in report['circuits']['inputs']
                  for depth in map(int,policy[suite]['depths'].split(',')) for polarity in (1,-1)}
        actual={(r['circuit'],r['flags'],r['depth'],r['polarity']) for r in report['runs']}
        assert len(report['circuits']['inputs'])==4 and actual==expected and len(report['runs'])==len(expected),'incomplete query set'
        for row in report['runs']:
            assert row['validated'],(suite,index,'unvalidated')
            key=(row['circuit'],row['flags'],row['depth'],row['polarity'])
            identities.setdefault(key,set()).add(row['input_sha256'])
            case=cases.setdefault(key,{'baseline':[],'candidate':[]})
            assert row['result'] in (0,10,20)
            assert math.isfinite(row['cpu_seconds']) and row['cpu_seconds']>=0
            if row['result']==20:assert row['certificate_checks']['bsat-check']['verified'] and row['bsat_proof_sha256']
            if row['result']==10:assert row['bsat_model_and_simulation']
            completed=bool(row['result'] and row['within_cpu_budget'] and row['cpu_seconds']<=policy[suite]['cpu'])
            value={'checked':completed,'result':row['result'],
                   'cpu':row['cpu_seconds'],'cpu_par2':row['cpu_seconds'] if completed else 2*policy[suite]['cpu'],
                   'conflicts':row['stats']['conflicts'],
                   'owned_capacity_bytes':row['stats']['owned_capacity_bytes'],
                   'journal_bytes':row.get('journal_bytes',0),
                   'export_cpu_seconds':row.get('export_cpu_seconds',0),
                   'bsat_check_wall_seconds':row.get('certificate_checks',{}).get('bsat-check',{}).get('seconds',0)}
            by_version[version].append(value);case[version].append(value)
    assert all(len(v)==1 for v in identities.values()),'query contexts differ'
    losses=[];gains=[];details=[]
    for key,variants in sorted(cases.items()):
        assert all(len(v)==2 for v in variants.values()),(key,'missing repeats')
        conclusive={r['result'] for rows in variants.values() for r in rows if r['result']}
        assert len(conclusive)<=1,(key,'contradictory checked answers')
        b=sum(r['checked'] for r in variants['baseline']);c=sum(r['checked'] for r in variants['candidate'])
        if c<b:losses.append(list(key))
        if c>b:gains.append(list(key))
        details.append({'circuit':key[0],'flags':key[1],'depth':key[2],'polarity':key[3],
                        'input_sha256':next(iter(identities[key])),
                        **{v:{'checked':sum(r['checked'] for r in rows),
                              'cpu_seconds':[r['cpu'] for r in rows],
                              'conflicts':[r['conflicts'] for r in rows],
                              'max_owned_capacity_bytes':max(r['owned_capacity_bytes'] for r in rows),
                              'max_journal_bytes':max(r['journal_bytes'] for r in rows),
                              'bsat_check_wall_seconds':[r['bsat_check_wall_seconds'] for r in rows],
                              'export_cpu_seconds':[r['export_cpu_seconds'] for r in rows]} for v,rows in variants.items()}})
    metrics={v:{'queries':len(rows),'checked':sum(r['checked'] for r in rows),
                'mean_cpu_par2':statistics.mean(r['cpu_par2'] for r in rows),
                'max_owned_capacity_bytes':max(r['owned_capacity_bytes'] for r in rows),
                'max_journal_bytes':max(r['journal_bytes'] for r in rows)} for v,rows in by_version.items()}
    ratio=metrics['candidate']['mean_cpu_par2']/metrics['baseline']['mean_cpu_par2']
    passed=not losses and ratio<=1.05
    summary['suites'][suite]={'metrics':metrics,'relative_cpu_par2_change':ratio-1,
                              'checked_solve_losses':losses,'checked_solve_gains':gains,
                              'gate_pass':passed,'queries':details}
    summary['gate_pass'] &= passed
(a.directory/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({k:{q:v for q,v in s.items() if q!='queries'} for k,s in summary['suites'].items()},indent=2))
# A failed performance gate is a measured outcome, not a harness failure.

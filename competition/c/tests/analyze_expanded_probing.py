#!/usr/bin/env python3
"""Summarize paired public certified histories, preserving UNKNOWN and late solves."""
import argparse
import hashlib
import json
from pathlib import Path


def summarize(data):
    if not data.get('complete') or data.get('accounting'):
        raise ValueError('requires complete, uninstrumented histories')
    budget=data['limits']['bsat_cpu']
    modes={3:{},7:{}}
    for row in data['runs']:
        if not row.get('validated') or row['flags'] not in modes:
            raise ValueError('unchecked row or unsupported mode')
        key=(row['circuit'],row['repeat'],row['depth'],row['polarity'])
        rows=modes[row['flags']]
        if key in rows:raise ValueError('duplicate query')
        rows[key]=row
        if row['result']==10 and not row.get('bsat_model_and_simulation'):
            raise ValueError('SAT needs original model and simulation checks')
        if row['result']==20 and not row.get('bsat_proof_sha256'):
            raise ValueError('UNSAT needs the public query certificate checked')
        if row['result'] not in (0,10,20):raise ValueError('invalid result')
    if not modes[3] or modes[3].keys()!=modes[7].keys():
        raise ValueError('unpaired histories')
    solved=lambda r:bool(r['result'] and r['cpu_seconds']<=budget)
    cost=lambda r:r['cpu_seconds'] if solved(r) else 2*budget
    lost=[];gained=[]
    for key,left in modes[3].items():
        right=modes[7][key]
        if left['input_sha256']!=right['input_sha256'] or left['assumptions']!=right['assumptions']:
            raise ValueError('different exact query')
        if left['result'] and right['result'] and left['result']!=right['result']:
            raise ValueError('answer disagreement')
        if solved(left) and not solved(right):lost.append(key)
        if solved(right) and not solved(left):gained.append(key)
    def aggregate(rows):
        rows=list(rows)
        return dict(queries=len(rows),solved=sum(solved(r) for r in rows),
                    sat=sum(r['result']==10 for r in rows),unsat=sum(r['result']==20 for r in rows),
                    unknown=sum(r['result']==0 for r in rows),
                    late=sum(bool(r['result']) and not solved(r) for r in rows),
                    total_cpu_par2=sum(cost(r) for r in rows),
                    peak_journal_bytes=max(r.get('journal_bytes',0) for r in rows),
                    peak_owned_capacity_bytes=max(r['stats']['owned_capacity_bytes'] for r in rows),
                    total_export_cpu_seconds=sum(r.get('export_cpu_seconds',0) for r in rows),
                    checkpoints=sum('checkpoint' in r for r in rows),
                    checkpoint_cpu_seconds=sum(r.get('checkpoint',{}).get('cpu_seconds',0) for r in rows))
    totals={f:aggregate(rows.values()) for f,rows in modes.items()}
    circuits={c:{f:aggregate(r for r in rows.values() if r['circuit']==c) for f,rows in modes.items()}
              for c in sorted({key[0] for key in modes[3]})}
    regression=totals[7]['total_cpu_par2']/totals[3]['total_cpu_par2']-1
    return dict(complete=True,modes=totals,circuits=circuits,lost_solves=lost,gained_solves=gained,
                cpu_par2_regression=regression,passes_no_loss_five_percent_gate=not lost and regression<=.05,
                scope='Observed paired history results; does not automatically promote a default')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--input',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();raw=a.input.read_bytes();result=summarize(json.loads(raw))
    result['source']=dict(path=str(a.input),sha256=hashlib.sha256(raw).hexdigest())
    a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result['modes'],indent=2))


if __name__=='__main__':main()

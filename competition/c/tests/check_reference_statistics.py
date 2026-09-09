#!/usr/bin/env python3
"""Check optional reference counters preserve changing-query IPASIR behavior."""
import argparse,json,hashlib,random
from pathlib import Path
from incremental_reference import Reference

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for k in ('baseline','diagnostic','output'):p.add_argument('--'+k,type=Path,required=True)
    a=p.parse_args();base=Reference(a.baseline.resolve(),30,60);diag=Reference(a.diagnostic.resolve(),30,60)
    rng=random.Random(9252);rows=[]
    try:
        for i in range(128):
            clause=[rng.choice((-1,1))*rng.randrange(1,33) for _ in range(3)]
            base.add(clause);diag.add(clause)
            assumptions=[rng.choice((-1,1))*rng.randrange(1,33) for _ in range(i%4)]
            before=diag.statistics();b=base.solve(assumptions);d=diag.solve(assumptions);after=diag.statistics()
            assert b==d and b in (10,20)
            assert all(after[k]>=before[k] for k in ('conflicts','decisions','propagations'))
            if b==10:assert base.model(32)==diag.model(32)
            else:
                for lit in assumptions:assert base.lib.ipasir_failed(base.s,lit)==diag.lib.ipasir_failed(diag.s,lit)
            rows.append({'result':b,'statistics':after})
        a.output.parent.mkdir(parents=True,exist_ok=True)
        a.output.write_text(json.dumps({'complete':True,'queries':len(rows),'model_and_core_parity':True,'baseline_sha256':hashlib.sha256(a.baseline.read_bytes()).hexdigest(),'diagnostic_sha256':hashlib.sha256(a.diagnostic.read_bytes()).hexdigest(),'runs':rows},indent=2)+'\n')
    finally:base.close();diag.close()
    print('PASS: 128 reference result/model/core queries with monotone counters')
if __name__=='__main__':main()

#!/usr/bin/env python3
"""Verify recovered child's exact retained input and independent certificates."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
from process_control import run_capture
from validate import parse_cnf, model_valid
from verified_check import verify

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary',required=True,type=Path)
p.add_argument('--output',required=True,type=Path)
a=p.parse_args()
sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
report=dict(complete=False,binary_sha256=sha(a.binary),checks=[])
a.output.parent.mkdir(parents=True,exist_ok=True)
try:
    with tempfile.TemporaryDirectory(prefix='bsat-recovery-') as tmp:
        root=Path(tmp);run=run_capture([str(a.binary.resolve()),str(root)],60)
        assert run.returncode==0,(run.stdout,run.stderr)
        report['supervisor']=json.loads(run.stdout)
        assert report['supervisor']['complete'] and report['supervisor']['accepted_attempt']==1
        assert not list(root.glob('0-*')), 'killed replay must not publish an answer'
        original=[[(-1 if bits&(1<<v) else 1)*(v+1) for v in range(6)] for bits in range(63)]+[[7]]
        for q in range(2):
            cnf,proof=root/f'1-{q}.cnf',root/f'1-{q}.drat'
            _,clauses=parse_cnf(cnf.read_text())
            assert clauses==original+([[-1]] if q==0 else [])
            row=dict(query=q,input_sha256=sha(cnf),proof_sha256=sha(proof))
            if q==0:
                checked=verify(cnf,proof,os.environ['BSAT_DRAT_TRIM'],os.environ['BSAT_CAKE_LPR'],root/'check',60)
                assert checked['verified'];row['verification']=checked
            else: assert model_valid(clauses,(root/'1-1.model').read_text())
            row['verified']=True;report['checks'].append(row)
    report['complete']=True
finally:a.output.write_text(json.dumps(report,indent=2)+'\n')
print('PASS: killed-worker replacement, cancellation/quota recovery and exact certified SAT/UNSAT snapshots')

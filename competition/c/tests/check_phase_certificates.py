#!/usr/bin/env python3
"""Independent SAT/model and DRAT-to-verified-LRAT checks for refreshed phase policies."""
import argparse
import hashlib
import itertools
import json
import os
from pathlib import Path
import subprocess
import tempfile
from validate import model_valid
from verified_check import verify


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--solver',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path)
    a=p.parse_args();rows=[]
    with tempfile.TemporaryDirectory(prefix='bsat-phase-cert-') as temp:
        root=Path(temp)
        for policy,interval,binary,sat in itertools.product(['--rephase-refresh','--rephase-diversify'],[1,1000],[False,True],[False,True]):
            holes,pigeons=(5,5) if sat else (4,5)
            clauses=[[p*holes+h+1 for h in range(holes)] for p in range(pigeons)]
            clauses += [[-(p*holes+h+1),-(q*holes+h+1)] for h in range(holes) for p in range(pigeons) for q in range(p)]
            cnf,proof=root/'input.cnf',root/'proof.drat'
            cnf.write_text(f'p cnf {holes*pigeons} {len(clauses)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in clauses))
            cmd=[str(a.solver.resolve()),policy,'--rephase-interval',str(interval),'--no-probing','--alternating','--proof',str(proof),str(cnf)]
            if binary:cmd.insert(1,'--binary-proof')
            run=subprocess.run(cmd,capture_output=True,text=True,timeout=30)
            assert run.returncode==(10 if sat else 20),(cmd,run.stdout,run.stderr)
            if sat:assert model_valid(clauses,run.stdout)
            else:assert verify(cnf,proof,os.environ['BSAT_DRAT_TRIM'],os.environ['BSAT_CAKE_LPR'],root/f'check-{len(rows)}',30)['verified']
            rows.append(dict(policy=policy,interval=interval,binary=binary,result=run.returncode,verified=True,
                             input_sha256=hashlib.sha256(cnf.read_bytes()).hexdigest(),proof_sha256=hashlib.sha256(proof.read_bytes()).hexdigest()))
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(dict(scope=__doc__,solver_sha256=hashlib.sha256(a.solver.read_bytes()).hexdigest(),runs=rows),indent=2)+'\n')
    print('PASS:',len(rows),'phase-policy models and independent verified certificates')


if __name__=='__main__':main()

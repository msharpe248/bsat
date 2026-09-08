#!/usr/bin/env python3
"""Independently verify certificates exported from retained incremental histories."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from validate import parse_cnf, model_valid
from verified_check import verify

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--driver',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    converter=os.environ['BSAT_DRAT_TRIM'];checker=os.environ['BSAT_CAKE_LPR'];rows=[]
    with tempfile.TemporaryDirectory(prefix='bsat-retained-cert-') as temp:
        root=Path(temp);r=subprocess.run([str(a.driver.resolve()),str(root)],capture_output=True,text=True,timeout=180)
        assert r.returncode==0,(r.stdout,r.stderr)
        for line in r.stdout.splitlines():
            q,status,reused,conflicts=map(int,line.split());folder=root/f'query-{q}';cnf=folder/'input.cnf';proof=folder/'proof.drat'
            row={'query':q,'result':status,'reused_preparations':reused,'conflicts':conflicts,'input_sha256':hashlib.sha256(cnf.read_bytes()).hexdigest(),'proof_sha256':hashlib.sha256(proof.read_bytes()).hexdigest(),'proof_bytes':proof.stat().st_size}
            if status==20:
                checked=verify(cnf,proof,converter,checker,folder/'check',120);assert checked['verified'],checked;row['validation']=checked
            else:
                _,cs=parse_cnf(cnf.read_text());assert model_valid(cs,(folder/'model.txt').read_text())
            row['verified']=True;rows.append(row)
        # Query 0 requires assumption -2; its proof must not certify satisfiable F.
        base=root/'wrong.cnf';base.write_text('p cnf 2 2\n1 2 0\n-1 2 0\n')
        assert not verify(base,root/'query-0/proof.drat',converter,checker,root/'wrong-check',30)['verified']
        # First 18 queries rebuild, next 18 retain. Both exact contexts agree.
        assert all(rows[i]['input_sha256']==rows[i+18]['input_sha256'] for i in range(18))
        assert rows[35]['reused_preparations']>0
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps({'scope':__doc__,'runs':rows,'wrong_context_rejected':True},indent=2)+'\n')
    print('PASS:',len(rows),'retained/rebuilt query certificates and wrong-context rejection')
if __name__=='__main__':main()

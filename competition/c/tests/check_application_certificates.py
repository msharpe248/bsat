#!/usr/bin/env python3
"""Check sampled >4000-variable queries from the actual retained app histories."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
from validate import parse_cnf,model_valid
from verified_check import verify

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--driver',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();rows=[]
    with tempfile.TemporaryDirectory(prefix='bsat-app-cert-') as temp:
        root=Path(temp)
        for kind in ['bmc','configuration']:
            r=subprocess.run([str(a.driver.resolve()),kind,'3',str(root)],capture_output=True,text=True,timeout=240);assert r.returncode==0,(r.stdout,r.stderr)
        for folder in sorted(root.iterdir()):
            cnf=folder/'input.cnf';proof=folder/'proof.drat';model=(folder/'model.txt').read_text();n,cs=parse_cnf(cnf.read_text());assert n>=4000
            row={'query':folder.name,'vars':n,'clauses':len(cs),'proof_bytes':proof.stat().st_size,'verified':True}
            if 's UNSATISFIABLE' in model:
                row['status']='UNSAT';row['validation']=verify(cnf,proof,os.environ['BSAT_DRAT_TRIM'],os.environ['BSAT_CAKE_LPR'],folder/'check',180);assert row['validation']['verified'],row
            else:row['status']='SAT';assert model_valid(cs,model)
            rows.append(row)
        assert len(rows)>=6 and any(x['status']=='UNSAT' for x in rows),rows
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps({'scope':__doc__,'runs':rows},indent=2)+'\n');print('PASS:',len(rows),'large retained application query certificates')
if __name__=='__main__':main()

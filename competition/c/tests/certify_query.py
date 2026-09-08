#!/usr/bin/env python3
"""Certify F AND assumptions as a separate, explicitly bound query artifact.

This never calls the unsupported conditional-proof C API. An UNSAT certificate
is evidence about query.cnf, not unconditional UNSAT of the base formula.
"""
import argparse
import math
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import time
from process_control import run_capture, interrupt_on_term
from validate import parse_cnf,cnf_text,model_valid
from verified_check import verify


def certify(source,assumptions,solver,converter,checker,directory,timeout=60,check_timeout=600):
    data=Path(source).read_bytes();n,clauses=parse_cnf(data.decode())
    if n<0 or n>((1<<29)-1) or any(not 0<abs(l)<=n for c in clauses for l in c):
        raise ValueError('invalid base literal or variable count')
    if any(not isinstance(l,int) or not 0<abs(l)<=n for l in assumptions):
        raise ValueError('assumptions must be nonzero existing DIMACS literals')
    directory=Path(directory);directory.mkdir(parents=True,exist_ok=False)
    base=directory/'base.cnf';query=directory/'query.cnf';proof=directory/'proof.drat'
    base.write_bytes(data);query.write_text(cnf_text(n,clauses+[[a] for a in assumptions]))
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    report={'schema':1,'scope':'base formula AND exact assumption units',
            'base_sha256':sha(base),'assumptions':list(assumptions),'query_sha256':sha(query),
            'solver_sha256':sha(Path(solver)),'status':'UNKNOWN','verified':False}
    start=time.monotonic()
    try:
        cmd=[str(solver),'--binary-proof','--proof',str(proof),str(query)]
        report['command']=cmd;begin=time.monotonic()
        try:run=run_capture(cmd,timeout)
        finally:report['solver_seconds']=time.monotonic()-begin
        (directory/'stdout').write_text(run.stdout);(directory/'stderr').write_text(run.stderr)
        report['returncode']=run.returncode
        report['proof_sha256']=sha(proof) if proof.exists() else None
        if run.returncode==10:
            report['status']='SAT';report['verified']=model_valid(clauses+[[a] for a in assumptions],run.stdout)
        elif run.returncode==20:
            report['status']='UNSAT'
            report['verification']=verify(query,proof,converter,checker,directory/'verification',check_timeout)
            report['verified']=report['verification']['verified']
        elif run.returncode!=0:report['status']='ERROR'
        if report['status'] in ('SAT','UNSAT') and not report['verified']:report['status']='ERROR'
    except subprocess.TimeoutExpired:
        report['timed_out']=True
    except (OSError,ValueError) as error:
        report['status']='ERROR';report['error']=str(error)
    finally:
        report['end_to_end_seconds']=time.monotonic()-start
        (directory/'query.json').write_text(json.dumps(report,indent=2)+'\n')
    return report


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('cnf',type=Path);p.add_argument('--assume',action='append',type=int,default=[])
    p.add_argument('--solver',required=True);p.add_argument('--converter',default=os.getenv('BSAT_DRAT_TRIM','drat-trim'))
    p.add_argument('--checker',default=os.getenv('BSAT_CAKE_LPR','cake_lpr'))
    p.add_argument('--artifacts',type=Path,required=True);p.add_argument('--timeout',type=float,default=60)
    p.add_argument('--check-timeout',type=float,default=600);args=p.parse_args()
    paths=[shutil.which(x) for x in (args.solver,args.converter,args.checker)]
    if not all(paths):p.error('solver, converter and verified checker must exist')
    if not all(math.isfinite(t) and t>0 for t in (args.timeout,args.check_timeout)):p.error('timeouts must be positive')
    try:r=certify(args.cnf,args.assume,*paths,args.artifacts,args.timeout,args.check_timeout)
    except (OSError,ValueError,AssertionError,UnicodeError) as e:p.error(str(e))
    print(json.dumps({k:r[k] for k in ('scope','status','verified','end_to_end_seconds')},indent=2))
    return (10 if r['status']=='SAT' else 20) if r['verified'] else 1 if r['status']=='ERROR' else 0


if __name__=='__main__':
    signal.signal(signal.SIGTERM,interrupt_on_term)
    raise SystemExit(main())

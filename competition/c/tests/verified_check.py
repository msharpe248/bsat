#!/usr/bin/env python3
"""Convert DRAT to LRAT, then accept only a successful cake_lpr check.

The converter is not the trusted acceptance oracle. cake_lpr checks the LRAT
against the exact snapshotted original CNF. This CLI is drat-trim-compatible for
benchmark.py: success prints exactly 's VERIFIED' and exits zero.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from validate import checker_verified


def cake_verified(run):
    statuses=[s for s in run.stdout.splitlines() if s.startswith('s ')]
    return run.returncode==0 and statuses==['s VERIFIED UNSAT']


def verify(cnf,drat,converter,checker,directory,timeout=600):
    directory=Path(directory);directory.mkdir(parents=True,exist_ok=True)
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    inp=directory/'input.cnf';proof=directory/'proof.drat';lrat=directory/'proof.lrat'
    shutil.copyfile(cnf,inp);shutil.copyfile(drat,proof)
    report={'input_sha256':sha(inp),'drat_sha256':sha(proof),
            'converter_sha256':sha(Path(converter)),'checker_sha256':sha(Path(checker)),
            'verified':False,'stages':[]}
    try:
        commands=[[str(converter),str(inp),str(proof),'-L',str(lrat)],
                  [str(checker),'--CML_HEAP_SIZE=512','--CML_STACK_SIZE=128',str(inp),str(lrat)]]
        for i,cmd in enumerate(commands):
            stage={'command':cmd};report['stages'].append(stage)
            try:
                run=subprocess.run(cmd,capture_output=True,text=True,timeout=timeout)
            except subprocess.TimeoutExpired as error:
                stage.update(timed_out=True,timeout=timeout,
                             stdout=(error.stdout or b'').decode(errors='replace') if isinstance(error.stdout,bytes) else error.stdout,
                             stderr=(error.stderr or b'').decode(errors='replace') if isinstance(error.stderr,bytes) else error.stderr)
                raise
            except OSError as error:
                stage['error']=str(error);raise
            stage.update(returncode=run.returncode,stdout=run.stdout,stderr=run.stderr)
            if i==0:
                if not checker_verified(run) or not lrat.is_file():return report
                report['lrat_sha256']=sha(lrat)
            else:report['verified']=cake_verified(run)
        return report
    finally:
        (directory/'verification.json').write_text(json.dumps(report,indent=2)+'\n')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('cnf',type=Path);p.add_argument('drat',type=Path)
    p.add_argument('--converter',default=os.getenv('BSAT_DRAT_TRIM','drat-trim'))
    p.add_argument('--cake-checker',default=os.getenv('BSAT_CAKE_LPR','cake_lpr'))
    p.add_argument('--artifacts',type=Path);p.add_argument('--timeout',type=float,default=600)
    args=p.parse_args();converter=shutil.which(args.converter);checker=shutil.which(args.cake_checker)
    if not converter or not checker:p.error('converter/checker executable missing')
    try:
        if args.artifacts:
            args.artifacts.mkdir(parents=True,exist_ok=True)
            folder=tempfile.mkdtemp(prefix='verified-',dir=args.artifacts)
            result=verify(args.cnf,args.drat,converter,checker,folder,args.timeout)
            print(f'c Verification artifacts: {folder}')
        else:
            with tempfile.TemporaryDirectory(prefix='bsat-verified-') as folder:
                result=verify(args.cnf,args.drat,converter,checker,folder,args.timeout)
        if result['verified']:print('s VERIFIED');return 0
        print('c Verified-checker chain rejected the certificate',file=sys.stderr);return 1
    except (OSError,subprocess.TimeoutExpired) as error:
        print(f'c Verification failed: {error}',file=sys.stderr);return 1


if __name__=='__main__':raise SystemExit(main())

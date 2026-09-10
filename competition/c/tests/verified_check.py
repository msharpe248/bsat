#!/usr/bin/env python3
"""Convert DRAT to LRAT, then accept only a successful cake_lpr check.

The converter is not the trusted acceptance oracle. cake_lpr checks the LRAT
against the exact snapshotted original CNF. This CLI is drat-trim-compatible for
benchmark.py: success prints exactly 's VERIFIED' and exits zero.
"""
import argparse
import math
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import resource
import signal
from process_control import run_capture, interrupt_on_term
from validate import checker_verified


def cake_verified(run):
    statuses=[s for s in run.stdout.splitlines() if s.startswith('s ')]
    return run.returncode==0 and statuses==['s VERIFIED UNSAT']


def verify(cnf,drat,converter,checker,directory,timeout=600,heap_mb=512,stack_mb=128):
    if any(not isinstance(n,int) or isinstance(n,bool) or n<=0 for n in (heap_mb,stack_mb)):
        raise ValueError('checker heap_mb and stack_mb must be positive integers')
    started=time.monotonic();parent_started=time.process_time()
    directory=Path(directory);directory.mkdir(parents=True,exist_ok=True)
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    inp=directory/'input.cnf';proof=directory/'proof.drat';lrat=directory/'proof.lrat'
    shutil.copyfile(cnf,inp);shutil.copyfile(drat,proof)
    report={'input_sha256':sha(inp),'drat_sha256':sha(proof),
            'converter_sha256':sha(Path(converter)),'checker_sha256':sha(Path(checker)),
            'verified':False,'checker_heap_mb':heap_mb,'checker_stack_mb':stack_mb,'stages':[]}
    try:
        commands=[[str(converter),str(inp),str(proof),'-L',str(lrat)],
                  [str(checker),f'--CML_HEAP_SIZE={heap_mb}',f'--CML_STACK_SIZE={stack_mb}',str(inp),str(lrat)]]
        for i,cmd in enumerate(commands):
            stage={'command':cmd};report['stages'].append(stage)
            stage_start=time.monotonic();before=resource.getrusage(resource.RUSAGE_CHILDREN)
            try:
                run=run_capture(cmd,timeout)
            except subprocess.TimeoutExpired as error:
                stage.update(timed_out=True,timeout=timeout,
                             stdout=(error.stdout or b'').decode(errors='replace') if isinstance(error.stdout,bytes) else error.stdout,
                             stderr=(error.stderr or b'').decode(errors='replace') if isinstance(error.stderr,bytes) else error.stderr)
                raise
            except OSError as error:
                stage['error']=str(error);raise
            finally:
                usage=resource.getrusage(resource.RUSAGE_CHILDREN)
                stage['seconds']=time.monotonic()-stage_start
                stage['cpu_seconds']=usage.ru_utime+usage.ru_stime-before.ru_utime-before.ru_stime
                stage['children_peak_rss_bytes']=usage.ru_maxrss*(1 if sys.platform=='darwin' else 1024)
            stage.update(returncode=run.returncode,stdout=run.stdout,stderr=run.stderr)
            if i==0:
                if not checker_verified(run) or not lrat.is_file():return report
                report['lrat_sha256']=sha(lrat)
            else:report['verified']=cake_verified(run)
        return report
    finally:
        report['seconds']=time.monotonic()-started
        report['parent_cpu_seconds']=time.process_time()-parent_started
        report['total_cpu_seconds']=report['parent_cpu_seconds']+sum(s.get('cpu_seconds',0) for s in report['stages'])
        report['rss_scope']='Cumulative child-process high-water RSS, not isolated per-stage peak'
        (directory/'verification.json').write_text(json.dumps(report,indent=2)+'\n')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('cnf',type=Path);p.add_argument('drat',type=Path)
    p.add_argument('--converter',default=os.getenv('BSAT_DRAT_TRIM','drat-trim'))
    p.add_argument('--cake-checker',default=os.getenv('BSAT_CAKE_LPR','cake_lpr'))
    p.add_argument('--checker-heap-mb',type=int,default=os.getenv('BSAT_CAKE_HEAP_MB','512'))
    p.add_argument('--checker-stack-mb',type=int,default=os.getenv('BSAT_CAKE_STACK_MB','128'))
    p.add_argument('--artifacts',type=Path,default=os.getenv('BSAT_VERIFY_ARTIFACTS'));p.add_argument('--timeout',type=float,default=600)
    args=p.parse_args();converter=shutil.which(args.converter);checker=shutil.which(args.cake_checker)
    if not converter or not checker:p.error('converter/checker executable missing')
    if not math.isfinite(args.timeout) or args.timeout<=0:p.error('timeout must be finite and positive')
    if args.checker_heap_mb<=0 or args.checker_stack_mb<=0:p.error('checker heap and stack must be positive')
    try:
        if args.artifacts:
            args.artifacts.mkdir(parents=True,exist_ok=True)
            folder=tempfile.mkdtemp(prefix='verified-',dir=args.artifacts)
            result=verify(args.cnf,args.drat,converter,checker,folder,args.timeout,args.checker_heap_mb,args.checker_stack_mb)
            print(f'c Verification artifacts: {folder}')
        else:
            with tempfile.TemporaryDirectory(prefix='bsat-verified-') as folder:
                result=verify(args.cnf,args.drat,converter,checker,folder,args.timeout,args.checker_heap_mb,args.checker_stack_mb)
        if result['verified']:print('s VERIFIED');return 0
        print('c Verified-checker chain rejected the certificate',file=sys.stderr);return 1
    except (OSError,subprocess.TimeoutExpired) as error:
        print(f'c Verification failed: {error}',file=sys.stderr);return 1


if __name__=='__main__':
    signal.signal(signal.SIGTERM,interrupt_on_term)
    raise SystemExit(main())

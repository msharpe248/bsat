#!/usr/bin/env python3
"""Compare direct LRAT acceptance with DRAT core/literal trimming then LRAT."""
import argparse,hashlib,json,time,resource,sys,math
from pathlib import Path
from process_control import run_capture
from validate import checker_verified
from verified_check import verify

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for k in ('input','proof','converter','checker','output'):p.add_argument('--'+k,type=Path,required=True)
    p.add_argument('--timeout',type=float,default=600);a=p.parse_args()
    if not math.isfinite(a.timeout) or a.timeout<=0:p.error('timeout must be finite and positive')
    a.output.mkdir(parents=True,exist_ok=False);sha=lambda x:hashlib.sha256(x.read_bytes()).hexdigest()
    report={'input_sha256':sha(a.input),'proof_sha256':sha(a.proof),'input_proof_bytes':a.proof.stat().st_size,'policy':'direct,trim,trim,direct; independently check against unchanged exact query; include trim cost','stage_timeout':a.timeout,'checker_heap_mb':2048,'checker_stack_mb':512,'complete':False,'runs':[]}
    try:
        for i,mode in enumerate(('direct','trim','trim','direct')):
            folder=a.output/str(i);folder.mkdir();start=time.monotonic();parent_started=time.process_time();before=resource.getrusage(resource.RUSAGE_CHILDREN);proof=a.proof
            row={'mode':mode};report['runs'].append(row)
            if mode=='trim':
                proof=folder/'compact.drat';command=[str(a.converter.resolve()),str(a.input.resolve()),str(a.proof.resolve()),'-l',str(proof.resolve()),'-C']
                t=time.monotonic();run=run_capture(command,a.timeout);assert checker_verified(run),run
                row['trim']={'command':command,'seconds':time.monotonic()-t,'stdout':run.stdout,'stderr':run.stderr,'returncode':run.returncode}
            checked=verify(a.input,proof,a.converter.resolve(),a.checker.resolve(),folder/'check',a.timeout,2048,512)
            assert checked['verified'];after=resource.getrusage(resource.RUSAGE_CHILDREN)
            row.update(verified=True,check=checked,proof_sha256=sha(proof),proof_bytes=proof.stat().st_size,wall_seconds=time.monotonic()-start,children_cpu_seconds=after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime,children_cumulative_peak_rss_bytes=after.ru_maxrss*(1 if sys.platform=='darwin' else 1024))
            row['parent_cpu_seconds']=time.process_time()-parent_started
            row['total_cpu_seconds']=row['children_cpu_seconds']+row['parent_cpu_seconds']
            print(mode,row['wall_seconds'],row['proof_bytes'],flush=True)
            (a.output/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
        report['complete']=True
    finally:(a.output/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()

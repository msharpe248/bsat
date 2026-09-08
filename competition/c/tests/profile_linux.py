#!/usr/bin/env python3
"""Serial, CPU-pinned perf stat measurements with independent answer checks.

Counters cover the whole userspace process, including parsing and proof output.
They do not isolate propagation; normalize only alongside exact search counters.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import random
import shlex
import shutil
import signal
import subprocess
import time
from compare_work import COUNTERS
from validate import checker_verified, model_valid, parse_cnf

EVENTS=('cycles:u','instructions:u','branches:u','branch-misses:u','cache-references:u','cache-misses:u')


def parse_perf(text,events=EVENTS):
    result={}
    for line in text.splitlines():
        fields=[x.strip() for x in line.split(';')]
        if len(fields)<3 or fields[2] not in events:continue
        name=fields[2]
        if name in result:raise ValueError(f'duplicate event {name}; select one CPU/PMU')
        try:
            value=float(fields[0]);percent=float(fields[4])
        except (ValueError,IndexError) as error:
            raise ValueError(f'event unavailable or malformed: {line}') from error
        if not math.isfinite(value) or value<0 or not 0<percent<=100:
            raise ValueError(f'invalid counter: {line}')
        result[name]={'count':value,'running_percent':percent}
    if set(result)!=set(events):raise ValueError('required PMU counters missing')
    return result


def sha(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda:source.read(1024*1024),b''):h.update(block)
    return h.hexdigest()


def run_group(cmd,timeout,env):
    with subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,
                          start_new_session=True,env=env) as child:
        try:out,err=child.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid,signal.SIGKILL);out,err=child.communicate()
            return {'returncode':child.returncode,'stdout':out,'stderr':err,'timed_out':True}
        return {'returncode':child.returncode,'stdout':out,'stderr':err,'timed_out':False}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--solver',action='append',required=True,help='NAME=EXECUTABLE')
    p.add_argument('--cpu',type=int,required=True);p.add_argument('--options',default='')
    p.add_argument('--conflicts',type=int,default=100000);p.add_argument('--repeats',type=int,default=3)
    p.add_argument('--seed',type=int,default=20261332);p.add_argument('--timeout',type=float,default=600)
    p.add_argument('--checker',required=True);p.add_argument('--checker-timeout',type=float,default=1200)
    p.add_argument('--proof-format',choices=['binary','text'],default='binary')
    p.add_argument('--output',type=Path,required=True);p.add_argument('inputs',nargs='+',type=Path)
    args=p.parse_args()
    if platform.system()!='Linux':p.error('real Linux perf hardware counters required; no synthetic fallback')
    if min(args.conflicts,args.repeats,args.timeout,args.checker_timeout)<=0:p.error('limits must be positive')
    if args.cpu not in os.sched_getaffinity(0):p.error('CPU is outside the allowed affinity set')
    perf=shutil.which('perf');taskset=shutil.which('taskset');checker=shutil.which(args.checker)
    if not all([perf,taskset,checker]):p.error('perf, taskset and checker must be executable')
    solvers={name:Path(path).resolve() for name,path in (x.split('=',1) for x in args.solver)}
    if len(solvers)!=len(args.solver):p.error('solver names must be unique')
    inputs=[x.resolve() for x in args.inputs];folder=args.output.resolve()
    folder.mkdir(parents=True,exist_ok=False)
    governor=Path(f'/sys/devices/system/cpu/cpu{args.cpu}/cpufreq/scaling_governor')
    report={'scope':__doc__,'platform':platform.platform(),'cpuinfo':Path('/proc/cpuinfo').read_text(),
            'perf_version':subprocess.check_output([perf,'--version'],text=True).strip(),
            'governor':governor.read_text().strip() if governor.exists() else None,
            'cpu':args.cpu,'events':EVENTS,'seed':args.seed,'conflicts':args.conflicts,'repeats':args.repeats,
            'options':shlex.split(args.options),'proof_format':args.proof_format,'timeout':args.timeout,
            'checker_timeout':args.checker_timeout,'checker_sha256':sha(checker),
            'solvers':{k:{'path':str(v),'sha256':sha(v)} for k,v in solvers.items()},
            'inputs':{str(x):sha(x) for x in inputs},'runs':[],'complete':False}
    (folder/'policy.json').write_text(json.dumps(report,indent=2)+'\n')
    jobs=[(name,inp,repeat) for name in solvers for inp in inputs for repeat in range(args.repeats)]
    random.Random(args.seed).shuffle(jobs);traces={};env=dict(os.environ,LC_ALL='C')
    try:
        for index,(name,inp,repeat) in enumerate(jobs):
            run_dir=folder/f'run-{index:03d}';run_dir.mkdir();proof=run_dir/'proof.drat';raw=run_dir/'perf.csv'
            cmd=[taskset,'-c',str(args.cpu),perf,'stat','--no-big-num','-x',';','-o',str(raw),
                 '-e',','.join(EVENTS),'--',str(solvers[name]),*report['options'],
                 '--conflicts',str(args.conflicts),'--proof',str(proof)]
            if args.proof_format=='binary':cmd+=['--binary-proof']
            cmd+=[str(inp)];start=time.monotonic();row=run_group(cmd,args.timeout,env)
            row.update(command=cmd,solver=name,input=str(inp),repeat=repeat,wall_seconds=time.monotonic()-start)
            report['runs'].append(row)
            if row['timed_out'] or row['returncode'] not in (0,10,20):raise ValueError('perf/solver failed; inspect retained logs')
            expected={0:'s UNKNOWN',10:'s SATISFIABLE',20:'s UNSATISFIABLE'}[row['returncode']]
            if [line for line in row['stdout'].splitlines() if line.startswith('s ')] != [expected]:
                raise ValueError('solver status/exit mismatch')
            row['perf']=parse_perf(raw.read_text());stats={}
            for line in row['stdout'].splitlines():
                if line.startswith('c ') and ':' in line:
                    key,value=line[2:].split(':',1);stats[key.strip()]=value.strip()
            row['work']={key:int(stats[key]) for key in COUNTERS};row['proof_sha256']=sha(proof)
            trace=(row['returncode'],row['work'],row['proof_sha256']);key=(name,str(inp))
            if key in traces and traces[key]!=trace:raise ValueError('repetition search/proof changed')
            traces[key]=trace;prop=row['work']['Propagations']
            row['events_per_propagation']={k:v['count']/prop if prop else None for k,v in row['perf'].items()}
            row['verified']=False
            if row['returncode']==10:
                _,clauses=parse_cnf(inp.read_text());row['verified']=model_valid(clauses,row['stdout'])
            elif row['returncode']==20:
                check=run_group([checker,str(inp),str(proof)],args.checker_timeout,env);row['check']=check
                row['verified']=not check['timed_out'] and checker_verified(subprocess.CompletedProcess([],check['returncode'],check['stdout'],check['stderr']))
            if row['returncode'] in (10,20) and not row['verified']:raise ValueError('answer verification failed')
            (folder/'results.json').write_text(json.dumps(report,indent=2)+'\n')
        report['complete']=True
    except Exception as error:
        report['error']=str(error);raise
    finally:(folder/'results.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':main()

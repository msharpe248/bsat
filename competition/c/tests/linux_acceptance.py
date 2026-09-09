#!/usr/bin/env python3
"""Provisional complete-transaction acceptance in Linux cgroups/private tmpfs."""
import argparse
import ctypes as C
import gzip
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import signal
import subprocess
import sys
import tempfile
import time


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def counters(path):return {k:int(v) for k,v in (line.split() for line in path.read_text().splitlines())}


def transaction(args):
    # Executed only in the dedicated mount namespace created by the parent.
    subprocess.run(['mount','-t','tmpfs','-o',f'size={args.storage},mode=1777','tmpfs','/tmp'],check=True)
    os.environ['TMPDIR']='/tmp';tempfile.tempdir=None
    from public_api import library,literals
    from validate import parse_cnf,model_valid
    from verified_check import verify
    start=time.monotonic();stages=[]
    def timed(name,fn):
        before=time.monotonic();cpu=time.process_time();value=fn()
        stages.append(dict(name=name,wall_seconds=time.monotonic()-before,cpu_seconds=time.process_time()-cpu))
        return value
    manifest=json.loads(args.manifest.read_text());case=next(c for c in manifest['cases'] if c['name']==args.case)
    packed=args.manifest.parent/case['file'];assert sha(packed)==case['compressed_sha256']
    data=timed('decompress',lambda:gzip.decompress(packed.read_bytes()))
    assert hashlib.sha256(data).hexdigest()==case['sha256']
    n,clauses=timed('parse',lambda:parse_cnf(data.decode()))
    lib=library(args.library);handle=None
    def rebuild():
        nonlocal handle
        if handle:lib.bsat_destroy(handle)
        handle=lib.bsat_create(1,3);assert handle
        assert lib.bsat_set_query_limits(handle,args.query_cpu,0,0)
        assert lib.bsat_set_journal_limit(handle,args.journal_mb*1024*1024)
        for clause in clauses:assert lib.bsat_add_clause(handle,literals(clause),len(clause))
    timed('load',rebuild)
    if args.fault=='kill':os.kill(os.getpid(),signal.SIGKILL)
    if args.fault=='memory':
        blocks=[]
        while True:blocks.append(bytearray(8*1024*1024))
    if args.fault=='cpu':
        while True:pass
    if args.fault=='wall':time.sleep(300)
    storage_failure=None
    if args.fault=='storage':
        path=Path('/tmp/fill');written=0
        try:
            with path.open('wb',buffering=0) as stream:
                while True:written+=stream.write(b'x'*(1024*1024))
        except OSError as error:
            import errno
            assert error.errno==errno.ENOSPC
        a=literals([case['assumptions'][0]])
        result=lib.bsat_solve(handle,a,1)
        assert result==0 and lib.bsat_error(handle) and not lib.bsat_value(handle,1)
        storage_failure=dict(bytes_written=written,result=result,error=bool(lib.bsat_error(handle)))
        path.unlink();timed('storage_replay',rebuild)
    answers=[]
    try:
        for index,assumption in enumerate(case['assumptions']):
            if index:assert lib.bsat_set_query_limits(handle,args.resume_query_cpu or args.query_cpu,0,0)
            arr=literals([assumption]);result=timed('solve',lambda:lib.bsat_solve(handle,arr,1))
            assert result in (0,10,20) and not lib.bsat_error(handle)
            row=dict(result=result,assumption=assumption,accepted=False)
            if result:
                exact=clauses+[[assumption]]
                with tempfile.TemporaryDirectory() as tmp:
                    root=Path(tmp);cnf=root/'query.cnf';proof=root/'query.drat'
                    assert timed('export',lambda:lib.bsat_export_query(handle,os.fsencode(cnf),os.fsencode(proof)))
                    def validate():
                        _,exported=parse_cnf(cnf.read_text());assert exported==exact
                        if result==10:
                            model='v '+' '.join(str(lib.bsat_value(handle,v) or v) for v in range(1,n+1))+' 0\n'
                            assert model_valid(exact,model)
                        else:
                            checked=verify(cnf,proof,args.converter,args.checker,root/'check',args.checker_wall,args.checker_heap_mb,args.checker_stack_mb)
                            row['verification']=checked;assert checked['verified'],checked
                    timed('validate',validate)
                    row.update(accepted=True,input_sha256=sha(cnf),proof_sha256=sha(proof))
            answers.append(row)
            # Checkpoint rebuilds obey the current CPU budget too. The tiny first
            # UNKNOWN injection must restore the normal budget before recovery.
            checkpoint_cpu=args.resume_query_cpu or args.query_cpu
            assert lib.bsat_set_query_limits(handle,checkpoint_cpu,0,0)
            row['checkpoint_cpu_budget']=checkpoint_cpu
            assert timed('checkpoint',lambda:lib.bsat_checkpoint(handle))
            used=C.c_uint64();assert lib.bsat_get_journal_bytes(handle,C.byref(used)) and used.value==0
            assert not lib.bsat_value(handle,1)
    finally:lib.bsat_destroy(handle)
    fs=os.statvfs('/tmp')
    report=dict(complete=True,case=args.case,source_sha256=case['sha256'],answers=answers,
                stages=stages,storage_failure=storage_failure,transaction_seconds=time.monotonic()-start,
                temporary_bytes_used=(fs.f_blocks-fs.f_bfree)*fs.f_frsize,
                temporary_capacity_bytes=fs.f_blocks*fs.f_frsize)
    print(json.dumps(report),flush=True)


def run_isolated(args,parent,name,case,fault='none',query_cpu=None):
    started=time.monotonic()
    cg=parent/name;cg.mkdir()
    memory=64*1024*1024 if fault=='memory' else args.memory_mb*1024**2
    for key,value in [('memory.max',memory),('memory.swap.max',0),('memory.oom.group',1),('pids.max',64)]:
        (cg/key).write_text(str(value))
    wall=1 if fault=='wall' else args.wall;cpu=1 if fault=='cpu' else args.aggregate_cpu
    command=['unshare','--mount','--propagation','private',sys.executable,str(Path(__file__).resolve()),
             '--isolated','--case',case,'--fault',fault,'--storage',str(args.storage),
             '--manifest',str(args.manifest),'--library',str(args.library),
             '--converter',str(args.converter),'--checker',str(args.checker),
             '--query-cpu',str(args.query_cpu if query_cpu is None else query_cpu),'--resume-query-cpu',str(args.query_cpu),
             '--journal-mb',str(args.journal_mb),'--checker-wall',str(args.checker_wall),
             '--checker-heap-mb',str(args.checker_heap_mb),'--checker-stack-mb',str(args.checker_stack_mb)]
    def attach():(cg/'cgroup.procs').write_text(str(os.getpid()))
    stop=None;storage_peak=0
    output=args.output/(name+'.stdout');errors=args.output/(name+'.stderr')
    try:
        with output.open('w') as out,errors.open('w') as err:
            child=subprocess.Popen(command,stdout=out,stderr=err,start_new_session=True,preexec_fn=attach,
                                   env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1'))
            while child.poll() is None:
                try:
                    fs=os.statvfs(f'/proc/{child.pid}/root/tmp')
                    if fs.f_blocks*fs.f_frsize==args.storage:
                        storage_peak=max(storage_peak,(fs.f_blocks-fs.f_bfree)*fs.f_frsize)
                except (FileNotFoundError,ProcessLookupError):pass
                usage=counters(cg/'cpu.stat')['usage_usec']/1e6
                if time.monotonic()-started>wall:stop='wall'
                elif usage>cpu:stop='cpu'
                if stop:
                    (cg/'cgroup.kill').write_text('1');break
                time.sleep(.02)
            child.wait(timeout=10)
        elapsed=time.monotonic()-started
        row=dict(name=name,case=case,fault=fault,returncode=child.returncode,wall_seconds=elapsed,stop=stop,
                 limits=dict(memory_bytes=memory,wall_seconds=wall,aggregate_cpu_seconds=cpu,storage_bytes=args.storage),
                 temporary_sampled_peak_bytes=storage_peak,storage_sample_interval_seconds=.02,
                 memory_peak_bytes=int((cg/'memory.peak').read_text()),memory_events=counters(cg/'memory.events'),
                 cpu=counters(cg/'cpu.stat'),stderr=errors.read_text()[-8000:])
        row['within_limits']=(elapsed<=wall and row['cpu']['usage_usec']<=cpu*1e6 and row['memory_peak_bytes']<=memory)
        if child.returncode==0:
            row['transaction']=json.loads(output.read_text());assert row['transaction']['complete']
        else:assert not output.read_text().strip(),'failed worker must not publish a complete transaction'
        return row
    finally:
        (cg/'cgroup.kill').write_text('1')
        for _ in range(100):
            if not (cg/'cgroup.procs').read_text().strip():break
            time.sleep(.01)
        cg.rmdir()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('library','converter','checker','manifest'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--output',type=Path);p.add_argument('--isolated',action='store_true')
    p.add_argument('--case',default='gen23');p.add_argument('--fault',default='none')
    p.add_argument('--storage',type=int,default=512*1024**2)
    p.add_argument('--query-cpu',type=float,default=5)
    p.add_argument('--resume-query-cpu',type=float,default=0,help='Budget for queries after the first; 0 uses query-cpu')
    p.add_argument('--hard-query-cpu',type=float,default=0,help='Also require checked cal3 with this positive solve budget')
    p.add_argument('--journal-mb',type=int,default=32)
    p.add_argument('--checker-wall',type=float,default=30)
    p.add_argument('--checker-heap-mb',type=int,default=512)
    p.add_argument('--checker-stack-mb',type=int,default=128)
    p.add_argument('--wall',type=float,default=60)
    p.add_argument('--aggregate-cpu',type=float,default=30)
    p.add_argument('--memory-mb',type=int,default=4096)
    args=p.parse_args()
    if any(not math.isfinite(x) or x<=0 for x in (args.query_cpu,args.checker_wall,args.wall,args.aggregate_cpu)):
        p.error('CPU/wall limits must be finite and positive')
    if any(not math.isfinite(x) or x<0 for x in (args.hard_query_cpu,args.resume_query_cpu)):p.error('invalid hard/resume query budget')
    if min(args.storage,args.journal_mb,args.checker_heap_mb,args.checker_stack_mb,args.memory_mb)<=0:p.error('memory/storage limits must be positive')
    for name in ('library','converter','checker','manifest'):setattr(args,name,getattr(args,name).resolve())
    if args.isolated:return transaction(args)
    if sys.platform!='linux' or os.geteuid()!=0:p.error('requires root on a disposable cgroup-v2 Linux runner')
    if not args.output:p.error('--output required')
    args.output.mkdir(parents=True,exist_ok=True)
    report=dict(complete=False,scope='Provisional hosted Linux acceptance, not deployment certification',
                resource_scope='Worker and checker process tree; lightweight supervisor outside cgroup',
                storage_scope='Configured tmpfs hard capacity; 20 ms sampled occupancy is a lower bound on peak',
                policy={k:getattr(args,k) for k in ('query_cpu','hard_query_cpu','journal_mb','checker_wall','checker_heap_mb','checker_stack_mb','wall','aggregate_cpu','memory_mb','storage')},
                platform=platform.platform(),revision=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip(),
                hashes={n:sha(getattr(args,n)) for n in ('library','converter','checker','manifest')},runs=[])
    parent=Path('/sys/fs/cgroup')/f'bsat-acceptance-{os.getpid()}'
    try:
        parent.mkdir();(parent/'cgroup.subtree_control').write_text('+memory +pids')
        for repeat in range(2):
            row=run_isolated(args,parent,f'normal-{repeat}','gen23');report['runs'].append(row)
            assert row['returncode']==0 and row['stop'] is None and row['within_limits']
            assert [r['result'] for r in row['transaction']['answers']]==[20,10]
            assert all(r['accepted'] for r in row['transaction']['answers'])
        if args.hard_query_cpu:
            for repeat in range(2):
                row=run_isolated(args,parent,f'hard-{repeat}','cal3',query_cpu=args.hard_query_cpu);report['runs'].append(row)
                assert row['returncode']==0 and row['stop'] is None and row['within_limits']
                assert [r['result'] for r in row['transaction']['answers']]==[20,10]
                assert all(r['accepted'] for r in row['transaction']['answers'])
        row=run_isolated(args,parent,'unknown','cal3',query_cpu=0.000001);report['runs'].append(row)
        assert row['returncode']==0 and row['within_limits']
        assert [r['result'] for r in row['transaction']['answers']]==[0,10]
        assert [r['accepted'] for r in row['transaction']['answers']]==[False,True]
        for fault in ('kill','memory','cpu','wall','storage'):
            before=time.monotonic();row=run_isolated(args,parent,fault,'gen23',fault);report['runs'].append(row)
            if fault=='storage':
                assert row['returncode']==0 and row['within_limits'] and row['transaction']['storage_failure']
                assert all(r['accepted'] for r in row['transaction']['answers'])
            else:
                assert row['returncode']!=0 and 'transaction' not in row
                if fault=='memory':assert row['memory_events']['oom_kill']>0
                if fault in ('cpu','wall'):assert row['stop']==fault
                retry=run_isolated(args,parent,fault+'-replay','gen23');report['runs'].append(retry)
                assert retry['returncode']==0 and retry['stop'] is None and retry['within_limits']
                assert all(r['accepted'] for r in retry['transaction']['answers'])
                assert row['cpu']['usage_usec']+retry['cpu']['usage_usec']<=args.aggregate_cpu*1e6
            row['recovery_total_seconds']=time.monotonic()-before
            assert row['recovery_total_seconds']<args.wall
        report['complete']=True
    except BaseException as error:
        report['failure']=repr(error);raise
    finally:
        (args.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
        if parent.exists():parent.rmdir()
    print('PASS: complete checked transactions and bounded failure/replay scenarios')


if __name__=='__main__':main()

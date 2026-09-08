#!/usr/bin/env python3
"""Serial fixed, growing and learning-heavy sessions with journal checkpoints."""
import argparse
import gzip
import ctypes as C
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import tempfile
import time
from public_api import Cancel, Stats, library, literals
from validate import parse_cnf, model_valid
from verified_check import verify


def percentile(values, p):
    return sorted(values)[max(0, math.ceil(len(values)*p)-1)] if values else None


def write_report(path, report):
    raw=(json.dumps(report,indent=2)+'\n').encode()
    path.write_bytes(gzip.compress(raw,mtime=0) if path.suffix=='.gz' else raw)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--library', required=True, type=Path)
    p.add_argument('--queries', type=int, default=8192)
    p.add_argument('--output', required=True, type=Path)
    a = p.parse_args()
    if a.queries < 64: p.error('at least 64 queries required')
    lib = library(a.library.resolve())
    report = dict(scope=__doc__, library_sha256=hashlib.sha256(a.library.read_bytes()).hexdigest(),
                  platform=platform.platform(), queries_per_session=a.queries, complete=False, sessions=[])
    a.output.parent.mkdir(parents=True, exist_ok=True)
    report['clock_scope']='perf_counter around public API call, including ctypes overhead; validation outside measured intervals'
    report['memory_scope']='owned core capacity, normalized by permanent input literals; excludes Python and transient/RSS costs'
    write_report(a.output,report)
    report['converter_sha256']=hashlib.sha256(Path(os.environ['BSAT_DRAT_TRIM']).read_bytes()).hexdigest()
    report['checker_sha256']=hashlib.sha256(Path(os.environ['BSAT_CAKE_LPR']).read_bytes()).hexdigest()
    for kind in ('fixed','growing','learning'):
        growing=kind=='growing'
        for watermark in ((8192,65536) if kind=='learning' else (256,2048)):
            s = lib.bsat_create(1, 3); assert s
            quota=1048576 if kind=='learning' else 131072
            assert lib.bsat_set_journal_limit(s, quota)
            assert lib.bsat_set_query_limits(s, 2, 0, 0)
            stop = [0]; callback = Cancel(lambda _: stop[0]);lib.bsat_set_terminate(s,None,callback)
            n, clauses, input_literals = 1, 0, 0
            original=[]
            def extend(target):
                nonlocal n, clauses, input_literals
                for v in range(n+1, target+1):
                    for c in ([v-1,-v],[-(v-1),v]):
                        assert lib.bsat_add_clause(s,literals(c),2);clauses += 1
                        original.append(c);input_literals+=len(c)
                n = target
            extend(1024 if growing else 4096 if kind=='learning' else 16384)
            if kind=='learning':
                first=n+1;guard=first+42
                for pigeon in range(7):
                    c=[-guard]+[first+pigeon*6+h for h in range(6)]
                    assert lib.bsat_add_clause(s,literals(c),len(c));clauses+=1;original.append(c);input_literals+=len(c)
                    for other in range(pigeon):
                        for h in range(6):
                            c=[-guard,-(first+pigeon*6+h),-(first+other*6+h)]
                            assert lib.bsat_add_clause(s,literals(c),len(c));clauses+=1;original.append(c);input_literals+=len(c)
                n=guard
            row = dict(kind=kind,growing=growing, watermark=watermark, journal_limit=quota,
                       queries=[], checkpoints=[], exports=[], cancellations=0, checkpoint_cancellations=0, wall_retries=0)
            try:
                with tempfile.TemporaryDirectory(prefix='bsat-session-') as temp:
                    folder = Path(temp)
                    for q in range(a.queries):
                        if growing and q % 256 == 0: extend(min(16384,1024+(q//256)*1024))
                        used = C.c_uint64();assert lib.bsat_get_journal_bytes(s,C.byref(used))
                        if used.value >= watermark:
                            start = time.perf_counter();assert lib.bsat_checkpoint(s)
                            row['checkpoints'].append(dict(query=q,seconds=time.perf_counter()-start,journal_before=used.value))
                            assert lib.bsat_get_journal_bytes(s,C.byref(used)) and used.value == 0
                        assumptions = [(-n if q%2 else n)] if kind=='learning' else [1,n if q%2 else -n]
                        arr = literals(assumptions);na=len(arr)
                        if q % 1021 == 0:
                            stop[0] = 1;assert not lib.bsat_checkpoint(s);stop[0] = 0
                            row['checkpoint_cancellations'] += 1
                            start=time.perf_counter();assert lib.bsat_checkpoint(s)
                            row['checkpoints'].append(dict(query=q,seconds=time.perf_counter()-start,journal_before=used.value,retry=True))
                        if q % 257 == 0:
                            stop[0] = 1;assert lib.bsat_solve(s,arr,na)==0;stop[0]=0;row['cancellations']+=1
                        if q % 509 == 0:
                            assert lib.bsat_set_service_limits(s,1e-12,0)
                            assert lib.bsat_solve(s,arr,na)==0 and lib.bsat_service_limit_hit(s)==1
                            assert lib.bsat_set_service_limits(s,0,0);row['wall_retries']+=1
                        start = time.perf_counter();result = lib.bsat_solve(s,arr,na);elapsed=time.perf_counter()-start
                        assert result == (10 if q % 2 else 20) and not lib.bsat_error(s)
                        stats=Stats();assert lib.bsat_get_stats(s,C.byref(stats),C.sizeof(stats))
                        assert lib.bsat_get_journal_bytes(s,C.byref(used)) and used.value<=quota
                        row['queries'].append(dict(query=q,variables=n,clauses=clauses,result=result,seconds=elapsed,
                            owned_bytes=stats.owned_capacity_bytes,journal_bytes=used.value,
                            conflicts=stats.conflicts,reused_preparations=stats.reused_preparations,
                            owned_bytes_per_input_literal=stats.owned_capacity_bytes/input_literals))
                        if q % 256 in (0,1):
                            cnf,proof=folder/f'{q}.cnf',folder/f'{q}.drat'
                            start=time.perf_counter();assert lib.bsat_export_query(s,os.fsencode(cnf),os.fsencode(proof))
                            export=dict(query=q,seconds=time.perf_counter()-start,cnf_bytes=cnf.stat().st_size,proof_bytes=proof.stat().st_size,
                                input_sha256=hashlib.sha256(cnf.read_bytes()).hexdigest(),proof_sha256=hashlib.sha256(proof.read_bytes()).hexdigest())
                            _,cs=parse_cnf(cnf.read_text());assert cs==original+[[v] for v in assumptions]
                            if result==10:
                                model='v '+' '.join(str(lib.bsat_value(s,v)) for v in range(1,n+1))+' 0\n'
                                assert model_valid(cs,model)
                            else:
                                checked=verify(cnf,proof,os.environ['BSAT_DRAT_TRIM'],os.environ['BSAT_CAKE_LPR'],folder/f'check-{q}',30)
                                assert checked['verified']
                            export['verified']=True;row['exports'].append(export)
                row['summary']={kind:{'count':len(rows),'p50':percentile([r['seconds'] for r in rows],.5),
                    'p95':percentile([r['seconds'] for r in rows],.95),'p99':percentile([r['seconds'] for r in rows],.99),
                    'max':max((r['seconds'] for r in rows),default=None)} for kind,rows in
                    [('query',row['queries']),('checkpoint',row['checkpoints']),('export',row['exports'])]}
                report['sessions'].append(row);write_report(a.output,report)
                print('kind',kind,'watermark',watermark,row['summary'],flush=True)
            except Exception as error:
                report['failure']=dict(kind=kind,watermark=watermark,error=repr(error),session=row)
                write_report(a.output,report);raise
            finally: lib.bsat_destroy(s)
    report['complete']=True;write_report(a.output,report)


if __name__ == '__main__': main()

#!/usr/bin/env python3
"""Serial changing-workload example comparison, exact sampled contexts and certificates."""
import argparse,hashlib,json,os,subprocess,tempfile,itertools
from pathlib import Path
from validate import parse_cnf,model_valid
from verified_check import verify
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--binary',required=True,type=Path);p.add_argument('--output',required=True,type=Path)
p.add_argument('--queries',type=int,default=1024);p.add_argument('--repeats',type=int,default=2)
a=p.parse_args();assert a.queries>=64 and a.repeats>0
sha=lambda f:hashlib.sha256(Path(f).read_bytes()).hexdigest()
report=dict(scope=__doc__,binary_sha256=sha(a.binary),queries=a.queries,repeats=a.repeats,
            quotas=[65536,1048576],reserve_floor="min(65536,quota/4)",soft_fraction=.25,checkpoint_cost_fraction=.01,
            converter_sha256=sha(os.environ['BSAT_DRAT_TRIM']),checker_sha256=sha(os.environ['BSAT_CAKE_LPR']),runs=[])
for quota,repeat in itertools.product(report['quotas'],range(a.repeats)):
    for fixed in ([False,True] if repeat%2==0 else [True,False]):
        with tempfile.TemporaryDirectory(prefix='bsat-policy-') as temp:
            root=Path(temp);env=dict(os.environ,BSAT_EXAMPLE_EXPORT_DIR=temp,BSAT_EXAMPLE_QUOTA=str(quota))
            cmd=[str(a.binary.resolve()),str(a.queries)]+(['f'] if fixed else [])
            result=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=120)
            assert result.returncode==0,(cmd,result.stdout,result.stderr)
            rows=[json.loads(line) for line in result.stdout.splitlines()]
            summary=rows.pop();assert summary['complete'] and len(rows)==a.queries
            clauses=[];guards=[];next_var=1;exports=[]
            for q,row in enumerate(rows):
                assert row['query']==q and row['result']==(10 if q%2 else 20) and row['journal']<=quota
                if q==0 or (len(guards)<4 and q>=len(guards)*(a.queries//4)):
                    pigeons=[5,6,7,6][len(guards)];holes=pigeons-1;first=next_var
                    guard=first+pigeons*holes;next_var=guard+1;guards.append(guard)
                    for pigeon in range(pigeons):
                        clauses.append([-guard]+[first+pigeon*holes+h for h in range(holes)])
                        for other in range(pigeon):
                            for h in range(holes):clauses.append([-guard,-(first+pigeon*holes+h),-(first+other*holes+h)])
                assumptions=[-g for g in guards];assumptions[-1]=(-guard if q%2 else guard)
                cnf=root/f'{q}.cnf'
                if not cnf.exists():continue
                n,actual=parse_cnf(cnf.read_text());assert n==next_var-1 and actual==clauses+[[v] for v in assumptions]
                record=dict(query=q,result=row['result'],input_sha256=sha(cnf))
                if row['result']==20:
                    checked=verify(cnf,root/f'{q}.drat',os.environ['BSAT_DRAT_TRIM'],os.environ['BSAT_CAKE_LPR'],root/f'check-{q}',30)
                    assert checked['verified'];record['proof_sha256']=checked['drat_sha256']
                else:assert model_valid(actual,(root/f'{q}.model').read_text())
                record['verified']=True;exports.append(record)
            assert len(exports)==8
            report['runs'].append(dict(quota=quota,repeat=repeat,command=cmd,summary=summary,queries=rows,exports=exports))
            a.output.write_text(json.dumps(report,indent=2)+'\n')
            print(quota,repeat,fixed,summary,flush=True)
report['complete']=True;a.output.write_text(json.dumps(report,indent=2)+'\n')

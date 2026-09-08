#!/usr/bin/env python3
"""Structured long-search and metamorphic checks with independent certificates."""
import argparse
from contextlib import contextmanager
import hashlib
import itertools
import json
from pathlib import Path
import random
import shutil
import subprocess
import tempfile
from validate import cnf_text, model_valid, checker_verified


@contextmanager
def workspace(output, report):
    with tempfile.TemporaryDirectory(prefix='bsat-long-search-') as tmp:
        try:
            yield Path(tmp)
        except BaseException:
            failures=Path('validation-failures');failures.mkdir(exist_ok=True)
            saved=Path(tempfile.mkdtemp(prefix='long-search-',dir=failures))
            shutil.copytree(tmp,saved,dirs_exist_ok=True)
            report['failure_artifacts']=str(saved.resolve())
            raise
        finally:
            output.parent.mkdir(parents=True,exist_ok=True)
            output.write_text(json.dumps(report,indent=2)+'\n')


def instance(sat, variant, seed, family):
    n=13
    clauses=[[(-v if bits>>(v-1)&1 else v) for v in range(1,n+1)]
             for bits in range((1<<n)-int(sat))]
    # Independent alias ensures SCC reconstruction runs alongside long search.
    clauses += [[-14,15],[14,-15]]
    if family == "pigeonhole":
        holes=7;pigeons=holes+int(not sat);n=holes*pigeons
        clauses=[[p*holes+h+1 for h in range(holes)] for p in range(pigeons)]
        clauses += [[-(p*holes+h+1),-(q*holes+h+1)] for h in range(holes) for p,q in itertools.combinations(range(pigeons),2)]
        clauses += [[-(n+1),n+2],[n+1,-(n+2)]]
    count=max(abs(l) for c in clauses for l in c)
    if variant:
        rng=random.Random(seed+variant);names=list(range(1,count+1));rng.shuffle(names)
        signs=[rng.choice([-1,1]) for _ in names]
        clauses=[[names[abs(l)-1]*signs[abs(l)-1]*(1 if l>0 else -1) for l in c] for c in clauses]
        for c in clauses:rng.shuffle(c)
        rng.shuffle(clauses)
    return count,clauses


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--solver',required=True);p.add_argument('--checker',required=True)
    p.add_argument('--seed',type=int,default=20261323);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();solver=str(Path(args.solver).resolve());checker=str(Path(args.checker).resolve())
    report={'seed':args.seed,'solver_sha256':hashlib.sha256(Path(solver).read_bytes()).hexdigest(),
            'checker_sha256':hashlib.sha256(Path(checker).read_bytes()).hexdigest(),'runs':[]}
    profiles=[[],['--chrono','--chrono-levels','0','--equiv','--alternating','--vmtf'],
              ['--equiv','--congruence','--dynamic-lbd','--protect-used','--reduce-increment','8']]
    totals={k:0 for k in ['Conflicts','Restarts','Reductions','Garbage collections','Substituted vars']}
    with workspace(args.output,report) as root:
        for family in ["exclusion", "pigeonhole"]:
            for profile,opts in enumerate(profiles):
                for variant in range(2):
                    for sat in [False,True]:
                        n,clauses=instance(sat,variant,args.seed,family);text=cnf_text(n,clauses)
                        inp=root/'input.cnf';inp.write_text(text);proof=root/'proof'
                        cmd=[solver,'--no-probing','--no-subsumption','--reduce-interval','32',
                             '--glucose-min-conflicts','16',*opts,'--proof',str(proof),str(inp)]
                        if variant:cmd.insert(1,'--binary-proof')
                        run=subprocess.run(cmd,capture_output=True,text=True,timeout=120)
                        row={'family':family,'profile':profile,'variant':variant,'sat':sat,'command':cmd,
                             'input_sha256':hashlib.sha256(inp.read_bytes()).hexdigest(),
                             'returncode':run.returncode,'stdout':run.stdout,'stderr':run.stderr}
                        report['runs'].append(row)
                        args.output.parent.mkdir(parents=True,exist_ok=True)
                        args.output.write_text(json.dumps(report,indent=2)+'\n')
                        assert run.returncode==(10 if sat else 20),row
                        assert 'AddressSanitizer' not in run.stderr and 'runtime error:' not in run.stderr,row
                        if sat:assert model_valid(clauses,run.stdout),row
                        else:
                            checked=subprocess.run([checker,str(inp),str(proof)],capture_output=True,text=True,timeout=120)
                            row['checker']={'returncode':checked.returncode,'stdout':checked.stdout,'stderr':checked.stderr}
                            assert checker_verified(checked),row
                        stats={}
                        for line in run.stdout.splitlines():
                            if line.startswith('c ') and ':' in line:
                                k,v=line[2:].split(':',1);stats[k.strip()]=v.strip()
                        row['stats']=stats;row['verified']=True
                        row['proof_sha256']=hashlib.sha256(proof.read_bytes()).hexdigest()
                        if not sat and family=="exclusion":assert int(stats['Conflicts'])>=1000,row
                        for k in totals:totals[k]+=int(stats[k])
                        print(f'PASS profile={profile} variant={variant} SAT={sat} conflicts={stats["Conflicts"]}',flush=True)
        # Require the relevant mechanisms to run, not merely correct easy answers.
        assert all(totals[k]>0 for k in totals),totals
        report['event_totals']=totals;report['complete']=True
        args.output.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: 24 structured/metamorphic cases with verified models/proofs and search-event coverage')


if __name__=='__main__':main()

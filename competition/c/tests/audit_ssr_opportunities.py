#!/usr/bin/env python3
"""Read-only, bounded SSR opportunity audit on normalized imported DIMACS.

No clauses are replaced. Counts are per original target, not a fixpoint or an
estimate of solver speed. Full occurrence indexing is used for small witnesses.
"""
import argparse, collections, hashlib, json
from pathlib import Path
from validate import parse_cnf


def audit(n, clauses, budget=1000000):
    if budget < 0 or n < 0: raise ValueError('negative bound')
    cs=[]
    for c in clauses:
        if any(not x or abs(x)>n for x in c): raise ValueError('literal outside DIMACS namespace')
        c=frozenset(c)
        cs.append(c if not any(-x in c for x in c) else None)
    occ=collections.defaultdict(list)
    for i,c in enumerate(cs):
        if c is not None:
            for x in c: occ[x].append(i)
    # Root closure is separate from SSR's inspection allowance.
    remaining=[len(c) if c is not None else 0 for c in cs]
    queue=collections.deque(next(iter(c)) for c in cs if c is not None and len(c)==1)
    assigned=set();root_work=0;root_unsat=any(c==frozenset() for c in cs)
    while queue and not root_unsat and root_work<budget:
        x=queue.popleft()
        if -x in assigned: root_unsat=True;break
        if x in assigned: continue
        assigned.add(x)
        for i in occ[-x]:
            root_work+=1
            if root_work>budget: break
            c=cs[i]
            if c & assigned: continue
            remaining[i]-=1
            if not remaining[i]: root_unsat=True;break
            if remaining[i]==1:
                queue.extend(y for y in sorted(c) if -y not in assigned)
        if root_work>budget: break
    root_complete=root_unsat or (not queue and root_work<=budget)
    witnesses=collections.defaultdict(list)
    for i,c in enumerate(cs):
        if c is not None and 2<=len(c)<=4:
            for x in c: witnesses[x].append(i)
    work=0;hits=[];eligible=0;complete=True;unassigned=0
    for ti,t in enumerate(cs):
        if t is None or not 3<=len(t)<=16: continue
        eligible+=1;found=False
        for pivot in sorted(t):
            for si in witnesses[-pivot]:
                source=cs[si];work+=1+len(source)
                if work>budget: complete=False;break
                if (source-{-pivot}) <= (t-{pivot}):
                    # Independently recheck signed relation without set subtraction.
                    assert -pivot in source and all(x==-pivot or (x in t and x!=pivot) for x in source)
                    free=not any(x in assigned or -x in assigned for x in t)
                    unassigned+=int(free)
                    hits.append({'source':si,'target':ti,'pivot':pivot,'root_unassigned':free})
                    found=True;break
            if found or not complete: break
        if not complete: break
    return {'variables':n,'clauses':len(cs),'normalization':'remove duplicates; exclude tautologies; preserve original clause indices',
            'eligible_targets_visited':eligible,'complete':complete,'budget':budget,'inspection_work':min(work,budget),
            'root_complete':root_complete,'root_unsat':root_unsat,'root_assigned_variables':len(assigned),
            'root_work':min(root_work,budget),'opportunity_targets':len(hits),'root_unassigned_opportunities':unassigned,
            'witnesses':hits,'scope':'One independently checked removal per target on immutable normalized input; no fixpoint, no speed claim. Root classification is provisional unless root_complete.'}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('inputs',nargs='+',type=Path);p.add_argument('--budget',type=int,default=1000000);p.add_argument('--output',required=True,type=Path);a=p.parse_args()
    rows=[]
    for path in a.inputs:
        data=path.read_bytes();n,cs=parse_cnf(data.decode());r=audit(n,cs,a.budget);r.update(input=str(path.resolve()),input_sha256=hashlib.sha256(data).hexdigest());rows.append(r)
    a.output.write_text(json.dumps({'scope':'Offline simplification opportunity audit; does not run or modify solver','runs':rows},indent=2)+'\n')
    print([(r['opportunity_targets'],r['root_unassigned_opportunities'],r['complete'],r['root_complete']) for r in rows])
if __name__=='__main__': main()

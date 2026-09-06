#!/usr/bin/env python3
"""Generate deterministic, disjoint development/heldout smoke benchmarks.

Synthetic smoke tests complement application and competition benchmarks; they
are not a substitute for representative production workloads.
"""
import argparse
import json
import random
from pathlib import Path
from validate import cnf_text


def generate(root, seed, count, heldout=False):
    if root.exists() and any(root.iterdir()):
        raise ValueError('output directory must be empty to avoid mixing benchmark splits')
    root.mkdir(parents=True, exist_ok=True)
    rng=random.Random(seed)
    entries=[]
    def save(family, name, n, clauses, expected=None):
        path=root/family/(name+'.cnf')
        path.parent.mkdir(parents=True,exist_ok=True)
        path.write_text(cnf_text(n,clauses))
        entries.append(dict(path=str(path.relative_to(root)),expected=expected))
    for n in (30,60,100):
        for i in range(count):
            clauses=[[v if rng.randrange(2) else -v for v in rng.sample(range(1,n+1),3)]
                     for _ in range(round(n*4.26))]
            save('random3sat',f'v{n}-{i}',n,clauses)
    for holes in ((3,4,5,6) if heldout else (2,7)):
        def v(p,h): return p*holes+h+1
        clauses=[[v(p,h) for h in range(holes)] for p in range(holes+1)]
        clauses += [[-v(p,h),-v(q,h)] for h in range(holes) for p in range(holes+1) for q in range(p+1,holes+1)]
        save('pigeonhole',f'php-{holes+1}-{holes}',holes*(holes+1),clauses,'UNSAT')
    for n in ((40,100,200) if heldout else (20,60,120)):
        # Two separately encoded XOR chains with equal input; assert outputs differ.
        clauses=[]
        def xor(a,b,c):
            clauses.extend([[a,b,-c],[-a,-b,-c],[a,-b,c],[-a,b,c]])
        left,right=1,1
        next_var=n+1
        for i in range(2,n+1):
            xor(left,i,next_var);left=next_var;next_var+=1
            xor(right,i,next_var);right=next_var;next_var+=1
        clauses.extend([[left,right],[-left,-right]])
        save('equivalence',f'xor-chain-{n}',next_var-1,clauses,'UNSAT')
    (root/'manifest.json').write_text(json.dumps(dict(seed=seed,instances=entries),indent=2)+'\n')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('output',type=Path)
    p.add_argument('--split',choices=('development','heldout'),default='development')
    p.add_argument('--count',type=int,default=5)
    a=p.parse_args()
    generate(a.output,20260906 if a.split=='development' else 20260907,a.count,a.split=='heldout')


if __name__=='__main__':main()

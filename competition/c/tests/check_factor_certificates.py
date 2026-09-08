#!/usr/bin/env python3
"""Check signed factoring rectangles and partial optimization prefixes."""
import argparse
import itertools
import json
from pathlib import Path
import subprocess
import tempfile

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--solver',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path)
    a=p.parse_args();rows=[]
    checker=Path(__file__).with_name('verified_check.py')
    with tempfile.TemporaryDirectory(prefix='bsat-factor-check-') as tmp:
        tmp=Path(tmp)
        for signs,fmt,cut,kind,gain in itertools.product(range(8),['text','binary'],[50,120,100000],['binary','ternary','two-column'],[1,4]):
            n=6 if kind=='binary' else 7 if kind=='two-column' else 9
            v=[(-i if signs>>(i%3)&1 else i) for i in range(1,n+1)]
            if kind=='binary':
                cs=[[x,y] for x in v[:3] for y in v[3:]]+[[-x for x in v[:3]],[-x for x in v[3:]]]
            else:
                cs=[[x,v[j],v[j+1]] for x in v[:3] for j in range(3,n,2)]
                cs += [[-x for x in v[:3]],[-v[j] for j in range(3,n,2)]]
                for j in range(3,n,2):cs += [[-v[j],v[j+1]],[v[j],-v[j+1]]]
            # A disjoint satisfiable rectangle also checks multiple fresh pivots.
            cs += [[n+x,n+y] for x in range(1,4) for y in range(4,7)]
            (tmp/'input').write_text(f'p cnf {n+6} {len(cs)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in cs))
            cmd=[str(a.solver.resolve()),'--factor','--no-probing','--factor-budget',str(cut),'--factor-min-gain',str(gain),'--proof',str(tmp/'proof')]
            if fmt=='binary':cmd+=['--binary-proof']
            r=subprocess.run(cmd+[str(tmp/'input')],capture_output=True,text=True,timeout=30)
            assert r.returncode==20,(cmd,r.stdout,r.stderr)
            c=subprocess.run([str(checker),str(tmp/'input'),str(tmp/'proof')],capture_output=True,text=True,timeout=120)
            assert c.returncode==0 and 's VERIFIED' in c.stdout,(c.stdout,c.stderr)
            rows.append({'kind':kind,'signs':signs,'format':fmt,'budget':cut,'min_gain':gain,'verified':True})
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps({'scope':__doc__,'runs':rows},indent=2)+'\n')
    print('PASS:',len(rows),'factoring certificates')

if __name__=='__main__':main()

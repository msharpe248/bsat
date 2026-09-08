#!/usr/bin/env python3
"""Independently certify representative queries from the hard history formula."""
import argparse
import json
from pathlib import Path
import tempfile
from certify_query import certify
from validate import cnf_text

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--solver',required=True,type=Path)
    p.add_argument('--converter',required=True,type=Path)
    p.add_argument('--checker',required=True,type=Path)
    p.add_argument('--output',required=True,type=Path)
    a=p.parse_args();clauses=[];selectors=[]
    for module in range(8):
        selector=1+57*module;selectors.append(selector);first=selector+1
        for pigeon in range(8):
            clauses.append([-selector]+[first+pigeon*7+h for h in range(7)])
        for h in range(7):
            for pigeon in range(8):
                for other in range(pigeon+1,8):
                    clauses.append([-selector,-(first+pigeon*7+h),-(first+other*7+h)])
    reports=[]
    with tempfile.TemporaryDirectory(prefix='bsat-hard-cert-') as tmp:
        tmp=Path(tmp);source=tmp/'history.cnf';source.write_text(cnf_text(456,clauses))
        for i,(assumptions,expected) in enumerate([
            ([selectors[0]],'UNSAT'),([selectors[-1]],'UNSAT'),
            ([-s for s in selectors],'SAT')]):
            result=certify(source,assumptions,a.solver.resolve(),a.converter.resolve(),
                           a.checker.resolve(),tmp/str(i),120,600)
            reports.append(result)
            a.output.parent.mkdir(parents=True,exist_ok=True)
            a.output.write_text(json.dumps({'scope':__doc__,'variables':456,
                'base_clauses':len(clauses),'queries':reports},indent=2)+'\n')
            assert result['status']==expected and result['verified'],result
    print('PASS: two hard conditional UNSAT LRAT chains and one original-query SAT model')

if __name__=='__main__':main()

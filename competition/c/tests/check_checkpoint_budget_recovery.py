#!/usr/bin/env python3
"""Regression: restore a normal CPU budget after an intentionally tiny query."""
import argparse,ctypes as C,gzip,json
from pathlib import Path
from public_api import library,literals
from validate import parse_cnf,model_valid
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--library',type=Path,required=True);a=p.parse_args()
manifest=Path(__file__).parent/'fixtures/acceptance/manifest.json';case=next(x for x in json.loads(manifest.read_text())['cases'] if x['name']=='cal3')
n,clauses=parse_cnf(gzip.decompress((manifest.parent/case['file']).read_bytes()).decode());lib=library(a.library.resolve());s=lib.bsat_create(1,3);assert s
try:
    for c in clauses:assert lib.bsat_add_clause(s,literals(c),len(c))
    assert lib.bsat_set_query_limits(s,0.000001,0,0)
    assert lib.bsat_solve(s,literals([case['assumptions'][0]]),1)==0 and not lib.bsat_error(s)
    assert lib.bsat_set_query_limits(s,60,0,0) and lib.bsat_checkpoint(s)
    count=C.c_uint64();assert lib.bsat_get_journal_bytes(s,C.byref(count)) and count.value==0
    assumption=case['assumptions'][1];assert lib.bsat_solve(s,literals([assumption]),1)==10
    model='v '+' '.join(str(lib.bsat_value(s,v) or v) for v in range(1,n+1))+' 0\n'
    assert model_valid(clauses+[[assumption]],model)
finally:lib.bsat_destroy(s)
print('PASS: tiny-budget UNKNOWN, restored-budget checkpoint and independently checked later SAT')

#!/usr/bin/env python3
"""Ensure the test facade's control preserves public results, work and exports."""
import argparse
import ctypes as C
import os
from pathlib import Path
import tempfile
import json
from public_api import library, literals, Stats

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--production',required=True,type=Path)
p.add_argument('--diagnostic',required=True,type=Path)
p.add_argument('--accounting',action='store_true',help='Enable intrusive counters during parity checks')
a=p.parse_args();base=library(a.production.resolve());diag=library(a.diagnostic.resolve())
diag.bsat_diagnostic_configure.argtypes=[C.c_void_p,C.c_char_p,C.c_int];diag.bsat_diagnostic_configure.restype=C.c_int
diag.bsat_diagnostic_write.argtypes=[C.c_void_p,C.c_char_p];diag.bsat_diagnostic_write.restype=C.c_int
assert not hasattr(base,'bsat_diagnostic_configure'),'test functions leaked into production'
with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp)
    for flags in range(4):
        handles=[(base,base.bsat_create(1,flags)),(diag,diag.bsat_create(1,flags))]
        assert all(s for _,s in handles)
        assert not diag.bsat_diagnostic_configure(handles[1][1],b'invalid',0)
        assert diag.bsat_diagnostic_configure(handles[1][1],b'control',int(a.accounting))
        try:
            for bits in range(63):
                c=[(-1 if bits&(1<<v) else 1)*(v+1) for v in range(6)]
                for lib,s in handles:assert lib.bsat_add_clause(s,literals(c),len(c))
            assert not diag.bsat_diagnostic_configure(handles[1][1],b'chrono',0)
            for q,assumptions in enumerate(([],[-1],[],[1],[-2],[])):
                rows=[];exports=[]
                for index,(lib,s) in enumerate(handles):
                    r=lib.bsat_solve(s,literals(assumptions),len(assumptions))
                    assert r==(20 if assumptions and assumptions[0]<0 else 10)
                    stats=Stats();assert lib.bsat_get_stats(s,C.byref(stats),C.sizeof(stats))
                    rows.append((r,stats.conflicts,stats.decisions,stats.propagations,[lib.bsat_value(s,v) for v in range(1,7)]))
                    if flags&2:
                        cnf=root/f'{flags}-{q}-{index}.cnf';proof=cnf.with_suffix('.drat')
                        assert lib.bsat_export_query(s,os.fsencode(cnf),os.fsencode(proof))
                        exports.append((cnf.read_bytes(),proof.read_bytes()))
                assert rows[0]==rows[1]
                if exports:assert exports[0]==exports[1]
            path=root/f'diagnostic-{flags}.json'
            assert diag.bsat_diagnostic_write(handles[1][1],os.fsencode(path))
            snapshot=json.loads(path.read_text())
            assert snapshot['conflicts']==rows[-1][1]
            assert snapshot['ordered_queue']==(flags==3)
            assert snapshot['retain_ternary']==(flags==3)
            assert not diag.bsat_diagnostic_write(handles[1][1],os.fsencode(path))
        finally:
            for lib,s in handles:lib.bsat_destroy(s)
print('PASS: 48 control queries, exact search work/model/export parity, exclusive snapshots, no diagnostic production exports')

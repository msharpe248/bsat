#!/usr/bin/env python3
"""macOS sampling of owned BSAT child processes; diagnostic, not timed scoring."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--solver',required=True,type=Path);p.add_argument('--output',required=True,type=Path)
    p.add_argument('inputs',nargs='+',type=Path);args=p.parse_args()
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    solver=args.solver.resolve();report={'solver_sha256':sha(solver),'runs':[],
        'scope':'Five-second macOS sample from process startup; 12-second solving CPU allowance. Sampling overhead and startup work included. No performance score or verified answer claim.'}
    with tempfile.TemporaryDirectory(prefix='bsat-profile-') as tmp:
        tmp=Path(tmp)
        for inp in args.inputs:
            inp=inp.resolve()
            for binary in [False,True]:
                cmd=[str(solver),'--chrono','--congruence','--equiv','--equiv-budget','100000000',
                     '--alternating','--vmtf','--time','12','--proof',str(tmp/'proof')]
                if binary:cmd+=['--binary-proof']
                cmd+=[str(inp)]
                with (tmp/'out').open('w') as out,(tmp/'err').open('w') as err:
                    child=subprocess.Popen(cmd,stdout=out,stderr=err)
                    try:
                        sampled=subprocess.run(['sample',str(child.pid),'5','1','-mayDie','-file',str(tmp/'sample')],
                                               capture_output=True,text=True,timeout=20)
                        assert sampled.returncode==0,(sampled.stdout,sampled.stderr)
                        child.wait(timeout=25)
                    finally:
                        if child.poll() is None:child.kill();child.wait()
                row={'input':str(inp),'input_sha256':sha(inp),'binary_proof':binary,'command':cmd,
                     'returncode':child.returncode,'stdout':(tmp/'out').read_text(),'stderr':(tmp/'err').read_text(),
                     'sample':(tmp/'sample').read_text(),'sampling_stderr':sampled.stderr}
                report['runs'].append(row);args.output.parent.mkdir(parents=True,exist_ok=True)
                args.output.write_text(json.dumps(report,indent=2)+'\n')
                print(f'Profiled {inp.parent.name}, binary={binary}, exit={child.returncode}',flush=True)
    report['complete']=True;args.output.write_text(json.dumps(report,indent=2)+'\n')


if __name__=='__main__':main()

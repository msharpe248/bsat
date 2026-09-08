#!/usr/bin/env python3
"""POSIX CLI interruption and proof-file exhaustion, with real child processes."""
import argparse
import itertools
from pathlib import Path
import resource
import signal
import subprocess
import tempfile
import time


def pigeonhole(pigeons=20):
    holes=pigeons-1
    clauses=[[p*holes+h+1 for h in range(holes)] for p in range(pigeons)]
    clauses += [[-(p*holes+h+1),-(q*holes+h+1)]
                for h in range(holes) for p,q in itertools.combinations(range(pigeons),2)]
    return f'p cnf {pigeons*holes} {len(clauses)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in clauses)


def limited_file():
    signal.signal(signal.SIGXFSZ,signal.SIG_IGN)
    resource.setrlimit(resource.RLIMIT_FSIZE,(1024,1024))


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--solver',required=True)
    args=p.parse_args();solver=str(Path(args.solver).resolve())
    with tempfile.TemporaryDirectory(prefix='bsat-process-failures-') as tmp:
        root=Path(tmp);inp=root/'input.cnf';inp.write_text(pigeonhole())
        for binary in [False,True]:
            proof=root/'proof';cmd=[solver,'--no-probing','--proof',str(proof)]
            if binary:cmd+=['--binary-proof']
            cmd+=[str(inp)]
            run=subprocess.run(cmd,preexec_fn=limited_file,capture_output=True,text=True,timeout=30)
            assert run.returncode==1 and 's UNKNOWN' in run.stdout,(run.returncode,run.stdout,run.stderr)
            assert not any(x in run.stdout for x in ['s SATISFIABLE','s UNSATISFIABLE'])
            assert 'certificate' in run.stderr and proof.stat().st_size<=1024,run.stderr
            for sig in [signal.SIGINT,signal.SIGTERM]:
                proof.unlink(missing_ok=True)
                child=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
                try:
                    deadline=time.monotonic()+10
                    # Proof bytes establish that search began; no signal-installation race.
                    while not proof.exists() or proof.stat().st_size==0:
                        assert child.poll() is None,'fixture completed before interruption'
                        assert time.monotonic()<deadline,'search did not produce proof bytes'
                        time.sleep(.01)
                    child.send_signal(sig);out,err=child.communicate(timeout=5)
                    assert child.returncode==-sig,(child.returncode,out,err)
                    assert not any(line.startswith('s ') for line in out.splitlines()),out
                    assert 'AddressSanitizer' not in err and 'runtime error:' not in err,err
                finally:
                    if child.poll() is None:child.kill();child.communicate()
    print('PASS: 2 proof-file exhaustion and 4 SIGINT/SIGTERM cases; no conclusive failed answers')


if __name__=='__main__':main()

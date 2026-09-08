#!/usr/bin/env python3
"""Build unmodified upstream genipaessentials against BSAT and check its answers."""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import random
import re
import subprocess
import tempfile

REVISION = '461a8f4611c41980037723d0e26856fb224ab188'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--upstream', required=True, type=Path)
    p.add_argument('--library', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    p.add_argument('--cxx', default='c++')
    a = p.parse_args()
    root, lib = a.upstream.resolve(), a.library.resolve()
    assert subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip() == REVISION
    source = root/'app/genipaessentials/genipaessentials.cpp'
    assert not subprocess.check_output(['git', '-C', str(root), 'diff', 'HEAD', '--', str(source), str(root/'ipasir.h')], text=True)
    sha = lambda f: hashlib.sha256(f.read_bytes()).hexdigest()
    report = dict(scope=__doc__, upstream_revision=REVISION, source_sha256=sha(source),header_sha256=sha(root/'ipasir.h'),
                  library_sha256=sha(lib), seed=2026090834, runs=[])
    with tempfile.TemporaryDirectory(prefix='bsat-upstream-client-') as temp:
        folder = Path(temp); binary = folder/'essentials'
        cmd = [a.cxx, '-O2', '-I'+str(root), str(source), str(lib),
               '-Wl,-rpath,'+str(lib.parent), '-o', str(binary)]
        subprocess.run(cmd, check=True, capture_output=True); report['build_command'] = cmd
        rng = random.Random(report['seed'])
        for case in range(65):
            n = 8 if case < 64 else 4096
            if case < 64:
                clauses = [[v * rng.choice([-1, 1]) for v in rng.sample(range(1,n+1), rng.randint(1,4))] for _ in range(20)]
                # Exact oracle: removing both polarities of x leaves a satisfiable
                # formula iff x is not essential in the client's partial-model sense.
                def satisfiable(cs):
                    return any(all(any(bits[abs(v)-1] == (v > 0) for v in c) for c in cs)
                               for bits in itertools.product((False, True), repeat=n))
                sat = satisfiable(clauses)
                expected = [v for v in range(1,n+1) if not satisfiable([[x for x in c if abs(x)!=v] for c in clauses])] if sat else []
            else:
                clauses = [[1]] + [[v, v+1] for v in range(2,n-1,2)] + [[n, 1]]
                sat, expected = True, [1]
            cnf = folder/'input.cnf'
            cnf.write_text(f'p cnf {n} {len(clauses)}\n'+''.join(' '.join(map(str,c))+' 0\n' for c in clauses))
            run = subprocess.run([str(binary), str(cnf)], capture_output=True, text=True, timeout=120)
            assert run.returncode == 0, run.stderr
            if sat:
                found = list(map(int,re.findall(r'^Variable (\d+) IS essential',run.stdout,re.M)))
                assert found == expected, (case,found,expected)
                assert len(re.findall(r'^Variable \d+ IS',run.stdout,re.M)) == n
            else: assert 'The input formula is unsatisfiable.' in run.stdout
            report['runs'].append(dict(case=case,variables=n,input_sha256=sha(cnf),satisfiable=sat,essential=expected,verified=True))
    report['complete'] = True
    a.output.parent.mkdir(parents=True,exist_ok=True); a.output.write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: 64 exact-oracle upstream client cases and a 4096-variable client history')


if __name__ == '__main__': main()

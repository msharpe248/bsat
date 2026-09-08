import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from verified_check import cake_verified, verify


class VerifiedTests(unittest.TestCase):
    def test_status_gate(self):
        for code,out,ok in [(0,'s VERIFIED UNSAT\n',True),(1,'s VERIFIED UNSAT\n',False),
                            (0,'c s VERIFIED UNSAT\n',False),(0,'s VERIFIED UNSAT\ns INVALID\n',False),
                            (-11,'s VERIFIED UNSAT\n',False),(0,'',False)]:
            self.assertEqual(cake_verified(subprocess.CompletedProcess([],code,out,'')),ok)

    def test_real_chain_and_corruption(self):
        solver=os.environ.get('BSAT_SOLVER');converter=os.environ.get('BSAT_DRAT_TRIM');checker=os.environ.get('BSAT_CAKE_LPR')
        if not all([solver,converter,checker]):self.skipTest('set BSAT_SOLVER, BSAT_DRAT_TRIM and BSAT_CAKE_LPR')
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);cnf=root/'in.cnf';proof=root/'in.drat'
            cases=['p cnf 1 1\n0\n','p cnf 1 2\n1 0\n-1 0\n',
                   'p cnf 2 4\n1 2 0\n1 -2 0\n-1 2 0\n-1 -2 0\n']
            for i,text in enumerate(cases):
                cnf.write_text(text)
                for binary in [False,True]:
                    cmd=[solver,'--no-probing','--proof',str(proof),str(cnf)]
                    if binary:cmd.insert(1,'--binary-proof')
                    run=subprocess.run(cmd,capture_output=True,text=True,timeout=10)
                    self.assertEqual(run.returncode,20,run.stderr)
                    folder=root/f'check-{i}-{binary}'
                    result=verify(cnf,proof,converter,checker,folder,30)
                    self.assertTrue(result['verified'],result)
                    self.assertGreater(result['seconds'],0)
                    self.assertEqual(len(result['stages']),2)
                    for stage in result['stages']:
                        self.assertGreater(stage['seconds'],0)
                        self.assertGreaterEqual(stage['cpu_seconds'],0)
                        self.assertGreater(stage['children_peak_rss_bytes'],0)
            # Directly reject an invalid LRAT derivation and a valid certificate
            # rebound to a satisfiable original input; status matching alone is insufficient.
            cnf.write_text('p cnf 2 1\n1 2 0\n');bad=root/'bad.lrat';bad.write_text('2 0 1 0\n')
            for candidate in [bad,root/'check-2-False'/'proof.lrat']:
                run=subprocess.run([checker,'--CML_HEAP_SIZE=128','--CML_STACK_SIZE=128',str(cnf),str(candidate)],capture_output=True,text=True,timeout=30)
                self.assertFalse(cake_verified(run),(run.stdout,run.stderr))
            proof.write_text('0\n')
            self.assertFalse(verify(cnf,proof,converter,checker,root/'bad-chain',30)['verified'])


if __name__=='__main__':unittest.main()

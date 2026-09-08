import os
from pathlib import Path
import tempfile
import unittest
from certify_query import certify
from verified_check import verify

class QueryTests(unittest.TestCase):
    def test_bound_queries_and_wrong_context(self):
        names=['BSAT_SOLVER','BSAT_DRAT_TRIM','BSAT_CAKE_LPR']
        if not all(os.getenv(k) for k in names):self.skipTest('set solver/converter/verified checker')
        solver,converter,checker=[str(Path(os.environ[k]).resolve()) for k in names]
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);base=root/'original.cnf';base.write_text('p cnf 2 2\n1 2 0\n-1 2 0\n')
            original=base.read_bytes()
            for i,(assumptions,status) in enumerate([([], 'SAT'),([2],'SAT'),([-2],'UNSAT'),([1,-1],'UNSAT'),([2,2],'SAT')]):
                r=certify(base,assumptions,solver,converter,checker,root/f'query{i}',10,30)
                self.assertEqual(r['status'],status,r);self.assertTrue(r['verified'],r)
                self.assertEqual(r['assumptions'],assumptions);self.assertEqual(base.read_bytes(),original)
                self.assertGreater(r['end_to_end_seconds'],r['solver_seconds'])
                if status=='UNSAT':
                    self.assertFalse(verify(base,root/f'query{i}'/'proof.drat',converter,checker,root/f'wrong{i}',30)['verified'])
            with self.assertRaises(FileExistsError):certify(base,[],solver,converter,checker,root/'query0')
            for bad in [[0],[3],[-2147483648]]:
                with self.assertRaises(ValueError):certify(base,bad,solver,converter,checker,root/'invalid')
            self.assertFalse((root/'invalid').exists())

if __name__=='__main__':unittest.main()

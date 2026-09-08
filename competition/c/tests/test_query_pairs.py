import copy
import unittest
from analyze_query_pairs import compare


class PairTests(unittest.TestCase):
    def reports(self):
        row=dict(circuit='x',flags=3,depth=1,polarity=1,repeat=0,validated=True,
                 within_cpu_budget=True,result=20,input_sha256='input',assumptions=[2],
                 diagnostic=dict(conflicts=7),fresh_proof_sha256='proof')
        a=dict(complete=True,accounting=False,diagnostic_profile='control',library_sha256='lib',limits={'cpu':5},runs=[row])
        b=copy.deepcopy(a);b['diagnostic_profile']='journal-off'
        return a,b
    def test_matching(self):
        a,b=self.reports();compare(a,b)
    def test_reject_mismatch_and_late_result(self):
        for field,value in [('within_cpu_budget',False),('validated',False),('input_sha256','wrong'),
                            ('diagnostic',{'conflicts':8}),('result',0),('assumptions',[-2])]:
            a,b=self.reports();b['runs'][0][field]=value
            with self.assertRaises(AssertionError):compare(a,b)
    def test_reject_duplicates_and_instrumentation(self):
        a,b=self.reports();b['runs']*=2
        with self.assertRaises(AssertionError):compare(a,b)
        a,b=self.reports();a['accounting']=True
        with self.assertRaises(AssertionError):compare(a,b)


if __name__=='__main__':unittest.main()

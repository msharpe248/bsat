#!/usr/bin/env python3
import copy,unittest
from summarize_competitiveness import summarize

def fixture():
    rows=[]
    for rep in (0,1):
        for depth in (0,2,4,8):
            for polarity in (1,-1):
                rows.append(dict(circuit='x',repeat=rep,flags=3,depth=depth,polarity=polarity,result=10,incremental_result=10,cpu_seconds=1.,incremental_cpu_seconds=2.,validated=True,input_sha256=str((depth,polarity)),bsat_model_and_simulation=True,incremental_model_and_simulation=True,stats={'owned_capacity_bytes':1},query_validation_wall_seconds=4.))
    return dict(complete=True,runs=rows,circuits={'inputs':[{'file':'x'}]},limits={'bsat_cpu':10,'cadical_thread_cpu':10})
class Summary(unittest.TestCase):
    def test_complete(self):self.assertEqual(summarize(fixture())['solvers']['bsat']['checked_within_budget'],16)
    def test_late(self):
        d=fixture();d['runs'][0]['cpu_seconds']=10.1
        self.assertEqual(summarize(d)['solvers']['bsat']['checked_within_budget'],15)
    def test_missing(self):
        d=fixture();d['runs'].pop()
        with self.assertRaises(AssertionError):summarize(d)
    def test_changed_context(self):
        d=fixture();d['runs'][0]['input_sha256']='changed'
        with self.assertRaises(AssertionError):summarize(d)
    def test_contradiction(self):
        d=fixture();d['runs'][0]['incremental_result']=20;d['runs'][0]['certificate_checks']={'fresh-check':{'verified':True}}
        with self.assertRaises(AssertionError):summarize(d)
    def test_nonfinite(self):
        d=fixture();d['runs'][0]['cpu_seconds']=float('nan')
        with self.assertRaises(AssertionError):summarize(d)
if __name__=='__main__':unittest.main()

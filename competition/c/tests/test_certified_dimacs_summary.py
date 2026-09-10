import copy, unittest
from benchmark_certified_dimacs import summarize
class Summary(unittest.TestCase):
    def rows(self):
        return [dict(version=v,result=10,verified=True,input='x',input_sha256='abc',load_cpu=.1,solve_cpu=1,export_cpu=.1,check_cpu=.1,within_budget=True,embedding_peak_rss_bytes=100) for v in ('baseline','candidate','candidate','baseline')]
    def test_equal(self):self.assertTrue(summarize(self.rows())['gate_pass'])
    def test_loss(self):
        r=self.rows();r[1]['result']=0;r[1]['verified']=False;self.assertFalse(summarize(r)['gate_pass'])
    def test_check_cost(self):
        r=self.rows();r[1]['check_cpu']=10;self.assertFalse(summarize(r)['gate_pass'])
    def test_invalid(self):
        for key,value in [('input_sha256','different'),('result',20),('solve_cpu',float('nan')),('verified',False)]:
            r=self.rows();r[1][key]=value
            with self.assertRaises(AssertionError):summarize(r)
if __name__=='__main__':unittest.main()

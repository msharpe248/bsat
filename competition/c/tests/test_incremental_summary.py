"""Acceptance-summary regressions: missing queries, context drift and masked losses."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT=Path(__file__).with_name('summarize_incremental_hosts.py')

class SummaryTests(unittest.TestCase):
    def evaluate(self,mutate=None):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            policy={'order':['baseline','candidate','candidate','baseline'],'flags':3,
                    'original':{'depths':'0','cpu':10},'expanded':{'depths':'0','cpu':10}}
            (root/'policy.json').write_text(json.dumps(policy))
            for suite in ('original','expanded'):
                for index,version in enumerate(policy['order']):
                    rows=[]
                    for circuit in range(4):
                        for polarity in (1,-1):
                            rows.append({'circuit':f'{circuit}.aig','flags':3,'depth':0,'polarity':polarity,
                                         'input_sha256':f'{circuit}-{polarity}','result':10,
                                         'validated':True,'within_cpu_budget':True,'cpu_seconds':9,
                                         'bsat_model_and_simulation':True,
                                         'stats':{'conflicts':1,'owned_capacity_bytes':100}})
                    report={'complete':True,'limits':{'bsat_cpu':10},
                            'circuits':{'inputs':[{'file':f'{i}.aig'} for i in range(4)]},'runs':rows}
                    if mutate:mutate(version,report)
                    (root/f'{suite}-{index}-{version}.json').write_text(json.dumps(report))
            run=subprocess.run([sys.executable,str(SCRIPT),str(root)],capture_output=True,text=True)
            path=root/'summary.json'
            return run.returncode,json.loads(path.read_text()) if path.exists() else None

    def test_equal(self):
        code,result=self.evaluate();self.assertEqual(code,0);self.assertTrue(result['gate_pass'])

    def test_loss_not_hidden_by_faster_other_queries(self):
        def change(version,r):
            if version=='candidate':
                for row in r['runs']:row['cpu_seconds']=1
                r['runs'][0].update(result=0,within_cpu_budget=False,cpu_seconds=10)
        code,result=self.evaluate(change);self.assertEqual(code,0);self.assertFalse(result['gate_pass'])
        s=result['suites']['original'];self.assertLess(s['relative_cpu_par2_change'],0)
        self.assertEqual(len(s['checked_solve_losses']),1)

    def test_late_conclusive_is_not_a_timed_solve(self):
        def change(version,r):
            if version=='candidate':
                for row in r['runs']:row['cpu_seconds']=11
        code,result=self.evaluate(change);self.assertEqual(code,0)
        self.assertEqual(result['suites']['original']['metrics']['candidate']['checked'],0)

    def test_context_drift_rejected(self):
        def change(version,r):
            if version=='candidate':r['runs'][0]['input_sha256']='different'
        code,_=self.evaluate(change);self.assertNotEqual(code,0)

    def test_missing_query_rejected(self):
        def change(version,r):
            if version=='candidate':r['runs'].pop()
        code,_=self.evaluate(change);self.assertNotEqual(code,0)

    def test_contradictory_checked_answers_rejected(self):
        def change(version,r):
            if version=='candidate':
                r['runs'][0].update(result=20,bsat_proof_sha256='proof',
                                    certificate_checks={'bsat-check':{'verified':True}})
        code,_=self.evaluate(change);self.assertNotEqual(code,0)

if __name__=='__main__':unittest.main()

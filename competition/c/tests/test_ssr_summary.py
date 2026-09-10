import json,subprocess,sys,tempfile,unittest
from pathlib import Path
class Gates(unittest.TestCase):
    def run_case(self,mutation=None):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            for target,depths in [('cal3',[0,1,2,4,8,16]),('cal100',[0,2,4])]:
                for index,version in enumerate(['baseline','candidate','candidate','baseline']):
                    rows=[]
                    for depth in depths:
                        for polarity in (1,-1):
                            r={'circuit':target+'.aig','flags':3,'depth':depth,'polarity':polarity,'validated':True,'result':20,'within_cpu_budget':True,'cpu_seconds':1.,'export_cpu_seconds':.1,'input_sha256':str((target,depth,polarity)),'bsat_proof_sha256':'proof','certificate_checks':{'bsat-check':{'verified':True,'total_cpu_seconds':2.,'seconds':2.}},'stats':{'owned_capacity_bytes':1},'diagnostic':{}}
                            if mutation:mutation(r,version,index)
                            rows.append(r)
                    (root/f'{target}-{index}-{version}.json').write_text(json.dumps({'complete':True,'runs':rows}))
            command=[sys.executable,str(Path(__file__).with_name('benchmark_ssr_targets.py')),'--summarize-only','--output',str(root)]
            for key in ('baseline','candidate','reference','incremental-reference','original','expanded'):command+=['--'+key,'unused']
            run=subprocess.run(command,capture_output=True,text=True)
            return run.returncode,json.loads((root/'summary.json').read_text()) if run.returncode==0 else None
    def test_equal(self):
        rc,d=self.run_case();self.assertEqual(rc,0);self.assertTrue(d['gate_pass']);self.assertFalse(d['benefit_demonstrated'])
    def test_loss(self):
        def change(r,v,i):
            if v=='candidate' and r['depth']==0 and r['polarity']==1:r.update(result=0,within_cpu_budget=False,cpu_seconds=60)
        rc,d=self.run_case(change);self.assertEqual(rc,0);self.assertFalse(d['gate_pass']);self.assertTrue(d['losses'])
    def test_check_cost(self):
        def change(r,v,i):
            if v=='candidate':r['certificate_checks']['bsat-check']['total_cpu_seconds']=10
        rc,d=self.run_case(change);self.assertEqual(rc,0);self.assertFalse(d['gate_pass'])
    def test_context(self):
        def change(r,v,i):
            if i==1:r['input_sha256']='different'
        self.assertNotEqual(self.run_case(change)[0],0)
    def test_nan(self):
        def change(r,v,i):r['cpu_seconds']=float('nan')
        self.assertNotEqual(self.run_case(change)[0],0)
if __name__=='__main__':unittest.main()

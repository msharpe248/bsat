import itertools, random, unittest
from audit_ssr_opportunities import audit

class Audit(unittest.TestCase):
    def test_exhaustive_random_oracle(self):
        rng=random.Random(4121)
        for _ in range(200):
            cs=[rng.sample([v if rng.randrange(2) else -v for v in range(1,6)],rng.randrange(2,6)) for j in range(12)]
            r=audit(5,cs);self.assertTrue(r['complete'])
            expected={i for i,t in enumerate(map(set,cs)) if 3<=len(t)<=16 and any(-p in s and s-{-p}<=t-{p} for p in t for s in map(set,cs) if 2<=len(s)<=4)}
            self.assertEqual({h['target'] for h in r['witnesses']},expected)
            for h in r['witnesses']:
                reduced=set(cs[h['target']])-{h['pivot']}
                for bits in itertools.product((False,True),repeat=5):
                    sat=lambda c:any(bits[abs(x)-1]==(x>0) for x in c)
                    self.assertFalse(sat(cs[h['source']]) and sat(cs[h['target']]) and not sat(reduced))
    def test_roots_and_limits(self):
        cs=[[-1,2],[1,2,3],[-2]]
        r=audit(3,cs);self.assertEqual(r['opportunity_targets'],1);self.assertEqual(r['root_unassigned_opportunities'],0);self.assertTrue(r['root_complete'])
        r=audit(3,cs,0);self.assertFalse(r['root_complete']);self.assertFalse(r['complete'])
        self.assertTrue(audit(1,[[1],[-1]])['root_unsat'])
        self.assertEqual(audit(3,[[-1,2],[1,2,3],[2],[-2]])['nontrivial_root_unassigned_opportunities'],0)
        self.assertIsNone(audit(3,cs,0)['nontrivial_root_unassigned_opportunities'])
    def test_normalization(self):
        r=audit(3,[[-1,2,2],[1,2,3],[1,-1,3]])
        self.assertEqual(r['opportunity_targets'],1)
        with self.assertRaises(ValueError):audit(2,[[3]])
if __name__=='__main__':unittest.main()

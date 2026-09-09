#!/usr/bin/env python3
import copy
import unittest
from analyze_expanded_probing import summarize


def fixture():
    rows=[]
    for flags in (3,7):
        rows.append(dict(circuit='fixture',repeat=0,depth=0,polarity=1,flags=flags,
                         result=20,cpu_seconds=.1,validated=True,input_sha256='same',
                         assumptions=[1],bsat_proof_sha256='checked',stats=dict(owned_capacity_bytes=100)))
    return dict(complete=True,accounting=False,limits=dict(bsat_cpu=1),runs=rows)


class SummaryTests(unittest.TestCase):
    def test_late_and_unknown_are_lost_solves(self):
        for result,cpu in [(0,.1),(20,1.01)]:
            data=fixture();data['runs'][1].update(result=result,cpu_seconds=cpu)
            out=summarize(data)
            self.assertEqual(len(out['lost_solves']),1)
            self.assertEqual(out['modes'][7]['total_cpu_par2'],2)
            self.assertFalse(out['passes_no_loss_five_percent_gate'])

    def test_rejects_unchecked_or_unpaired_evidence(self):
        for field,value in [('validated',False),('input_sha256','different'),
                            ('assumptions',[-1]),('bsat_proof_sha256',None)]:
            data=fixture();data['runs'][1][field]=value
            with self.assertRaises(ValueError):summarize(data)
        data=fixture();data['runs'].append(copy.deepcopy(data['runs'][0]))
        with self.assertRaises(ValueError):summarize(data)

    def test_matching_pair(self):
        out=summarize(fixture());self.assertEqual(out['lost_solves'],[])
        self.assertEqual(out['cpu_par2_regression'],0)
        self.assertTrue(out['passes_no_loss_five_percent_gate'])


if __name__=='__main__':unittest.main()
